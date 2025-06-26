#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <stddef.h>

#include "node-whisperer.h"
#include "daemon.h"
#include "util.h"

#define UPDATE_INTERVAL (30 * 1000)

#define BEACON_BUFFER_SIZE 512

extern struct nw_information_source information_sources[];

static int nw_daemon_create_vendor_element_buf(struct nw *instance) {
	uint8_t *element_length;
	char hexbuf[BEACON_BUFFER_SIZE * 2 + 1];

	/* Free buffer if it exists*/
	if (instance->output.buf) {
		free(instance->output.buf);
	}

	/* Allocate Buffer and Init sizes */
	instance->output.buf = calloc(BEACON_BUFFER_SIZE, 1);
	if (!instance->output.buf) {
		return -1;
	}
	instance->output.size = BEACON_BUFFER_SIZE;
	instance->output.len = 0;

	/* Tag-Type(1) + Teg-Length(1) + OUI(3) + type(1) + {data} */
	instance->output.len = 1 + 1 + 3 + 1;

	/* Header */
	instance->output.buf[0] = 0xDD;
	instance->output.buf[1] = 0x00; /* Set Later! */
	/* OUI */
	instance->output.buf[2] = 0x00;
	instance->output.buf[3] = 0x20;
	instance->output.buf[4] = 0x91;
	/* OUI-Type */
	instance->output.buf[5] = 0x04;

	/* Length now matches content. Make sure to update on each new information! */

	/* Loop through all information-sources */
	for (int i = 0; information_sources[i].name; i++) {
		if (!information_sources[i].enabled) {
			log_debug("Information source id=%d name=%s is disabled",
				  information_sources[i].type, information_sources[i].name);
			continue;
		} else {
			log_debug("Information source id=%d name=%s is enabled",
				  information_sources[i].type, information_sources[i].name);
		}

		/* Check if we have space for T + L + {data} */
		if (instance->output.len + 3 > instance->output.size) {
			log_error("Buffer too small for id=%d name=%s",
				  information_sources[i].type, information_sources[i].name);
			break;
		}

		/* Set T and L placeholder */
		instance->output.buf[instance->output.len] = information_sources[i].type;
		instance->output.buf[instance->output.len + 1] = 0x00;

		/* Save for later */
		element_length = &instance->output.buf[instance->output.len + 1];

		/* Collect Information */
		int ret = information_sources[i].collect(&instance->output.buf[instance->output.len + 2],
							 instance->output.size - instance->output.len - 2);
		if (ret == 0) {
			/* No Information available */
			log_error("No Information available for id=%d name=%s",
				  information_sources[i].type, information_sources[i].name);
			continue;
		} else if (ret > 0xff) {
			/* Too much Information */
			log_error("Too much Information for id=%d name=%s",
				  information_sources[i].type, information_sources[i].name);
			return -ENOMEM;
		} else if (ret < 0) {
			/* Error */
			log_error("Error collecting Information for id=%d name=%s code=%d",
				  information_sources[i].type, information_sources[i].name, ret);
			continue;
		} else {
			nw_buffer_to_hexstring(&instance->output.buf[instance->output.len + 2], ret, hexbuf);
			/* Information available */
			log_debug("Information available for id=%d name=%s element_length=%d content=%s",
				  information_sources[i].type, information_sources[i].name, ret, hexbuf);
		}

		/* Update Length of Field*/
		*element_length = (uint8_t)ret;

		/* Update total Length */
		instance->output.len += ret + 2;

		log_debug("Add Element to beacon id=%d name=%s element_length=%d total_length=%d",
			  information_sources[i].type, information_sources[i].name, ret, instance->output.len);
	}

	/* Set Length */
	instance->output.buf[1] = instance->output.len - 2;
	log_debug("Set length of beacon element element_length=%u total_length=%d",
		  instance->output.buf[1], instance->output.len);

	return 0;
}

static void nw_daemon_collect_information(struct uloop_timeout *timeout) {
	struct nw *instance = container_of(timeout, struct nw, update_timeout);

	if (nw_daemon_create_vendor_element_buf(instance) < 0) {
		goto out_free;
	}

	/* Allocate output buffer */
	size_t buf_len = instance->output.len * 2 + 1;
	uint8_t *buf_hex = malloc(buf_len);

	nw_buffer_to_hexstring(instance->output.buf, instance->output.len, buf_hex);

	/* Update nodes */
	log_debug("Update %s", buf_hex);
	nw_interface_update(&instance->ubus_ctx, (char *)buf_hex);

	instance->statistics.update_count++;

out_free:
	free(buf_hex);
	uloop_timeout_set(timeout, UPDATE_INTERVAL);
}

static int start_daemon() {
	struct nw instance = {};

	INIT_LIST_HEAD(&instance.enabled_interfaces);

	uloop_init();

	if (ubus_connect_ctx(&instance.ubus_ctx, NULL)) {
		log_error("Failed to connect to ubus");
		return -1;
	}

	/* Init ubus */
	ubus_add_uloop(&instance.ubus_ctx);
	nw_ubus_init(&instance.ubus_ctx);

	/* Add Information gathering timer */
	instance.update_timeout.cb = nw_daemon_collect_information;
	uloop_timeout_set(&instance.update_timeout, 5 * 1000);

	uloop_run();

	/* Terminate */
	uloop_done();
	return 0;
}

int main(int argc, char *argv[]) {
	int opt;

	while ((opt = getopt(argc, argv, "l:s")) != -1) {
		switch (opt) {
		case 'l':
			log_set_level(atoi(optarg));
			break;
		case 's':
			log_use_syslog(1);
			break;
		default:
			fprintf(stderr, "Usage: %s [-l loglevel] [-s]\n", argv[0]);
			exit(EXIT_FAILURE);
		}
	}

	start_daemon();
	return 0;
}
