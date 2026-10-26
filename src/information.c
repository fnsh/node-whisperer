#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <stddef.h>

#include <arpa/inet.h>
#include <sys/sysinfo.h>

#include "batadv.h"
#include "util.h"
#include "information.h"

#ifndef BUILD_MONITOR
#include <uci.h>
#include <json-c/json.h>
#include <libgluonutil.h>
#endif

#ifndef BUILD_MONITOR
#define INFORMATION_SOURCE(__name, __type) \
	{ \
		.name = #__name, \
		.type = __type, \
		.collect = node_whisperer_information_##__name##_collect, \
	}
#else
#define INFORMATION_SOURCE(__name, __type) \
	{ \
		.name = #__name, \
		.type = __type, \
		.parse = node_whisperer_information_##__name##_parse, \
	}
#endif

#ifndef BUILD_MONITOR
int node_whisperer_information_hostname_collect(uint8_t *buffer, size_t buffer_size) {
	int ret;

	ret = gethostname((char *)buffer, buffer_size);
	if (ret) {
		return ret;
	}

	return strlen((char *)buffer);
}

int node_whisperer_information_node_id_collect(uint8_t *buffer, size_t buffer_size) {
	char *node_id_ascii;
	size_t node_id_ascii_len;
	int ret;

	/* len_out = ETH_ALEN */
	if (buffer_size < 6) {
		return -1;
	}

	ret = nw_read_file("/lib/gluon/core/sysconfig/primary_mac", &node_id_ascii, &node_id_ascii_len);
	if (ret) {
		return ret;
	}

	if (node_id_ascii_len < 18) {
		free(node_id_ascii);
		return -1;
	}

	ret = nw_parse_mac_address_ascii(node_id_ascii, buffer);
	free(node_id_ascii);
	if (ret < 0) {
		return -1;
	}

	return 6;
}

int node_whisperer_information_batman_adv_collect(uint8_t *buffer, size_t buffer_size) {
	struct nw_batadv_neighbor_stats stats = {};
	uint16_t tmp;
	uint16_t num_clients;
	int ret;

	if (buffer_size < 8) {
		return -1;
	}

	ret = nw_get_batadv_neighbor_stats(&stats);
	if (ret)
		return -1;

	ret = nw_get_batadv_clients();
	if (ret < 0) {
		num_clients = 0;
	} else {
		num_clients = (uint16_t)ret;
	}

	buffer[0] = stats.vpn.connected ? 1 : 0;
	buffer[1] = stats.vpn.tq;

	tmp = htons(stats.originator_count);
	memcpy(&buffer[2], &tmp, sizeof(tmp));

	tmp = htons(stats.neighbor_count);
	memcpy(&buffer[4], &tmp, sizeof(tmp));

	tmp = htons(num_clients);
	memcpy(&buffer[6], &tmp, sizeof(tmp));

	return 8;
}

int node_whisperer_information_uptime_collect(uint8_t *buffer, size_t buffer_size) {
	struct sysinfo s_info;
	uint32_t uptime_minutes;
	int ret;

	ret = sysinfo(&s_info);
	if (ret) {
		return -1;
	}

	uptime_minutes = htonl(s_info.uptime / 60);

	memcpy(buffer, &uptime_minutes, sizeof(uptime_minutes));

	return sizeof(uptime_minutes) / sizeof(uint8_t);
}

int node_whisperer_information_site_code_collect(uint8_t *buffer, size_t buffer_size) {
	struct json_object *site;
	struct json_object *site_code_j;
	const char *site_code = NULL;
	size_t sice_code_len;
	int ret;

	site = gluonutil_load_site_config();
	if (!site) {
		ret = -1;
		goto out_free;
	}

	site_code_j = json_object_object_get(site, "site_code");
	if (!site_code_j) {
		ret = -1;
		goto out_free;
	}

	site_code = json_object_get_string(site_code_j);
	if (!site_code) {
		ret = -1;
		goto out_free;
	}

	sice_code_len = strlen(site_code);
	if (sice_code_len > buffer_size) {
		ret = -1;
		goto out_free;
	}

	memcpy(buffer, site_code, sice_code_len);
	ret = sice_code_len;

out_free:
	if (site)
		json_object_put(site);
	return ret;
}

int node_whisperer_information_domain_collect(uint8_t *buffer, size_t buffer_size) {
	char *dom;
	size_t dom_len;
	int ret;

	dom = gluonutil_get_domain();
	if (!dom) {
		ret = -1;
		goto out_free;
	}

	dom_len = strlen(dom);
	if (dom_len > buffer_size) {
		ret = -1;
		goto out_free;
	}

	memcpy(buffer, dom, dom_len);
	ret = dom_len;

out_free:
	if (dom)
		free(dom);
	return ret;
}

int node_whisperer_information_system_load_collect(uint8_t *buffer, size_t buffer_size) {
	double samples[3];

	if (buffer_size < 1) {
		return -1;
	}

	if (getloadavg(samples, 3) < 3) {
		return -1;
	}

	buffer[0] = (uint8_t) (samples[0] * 10);
	return 1;
}

int node_whisperer_information_firmware_version_collect(uint8_t *buffer, size_t buffer_size) {
	char *firmware_version;
	size_t firmware_version_len;
	int ret;

	ret = nw_read_file("/lib/gluon/release", &firmware_version, &firmware_version_len);
	if (ret) {
		return ret;
	}

	if (firmware_version_len > buffer_size) {
		free(firmware_version);
		return -1;
	}

	if (firmware_version_len > 1 && firmware_version[firmware_version_len - 1] == '\n') {
		firmware_version_len--;
	}

	memcpy(buffer, firmware_version, firmware_version_len);
	free(firmware_version);

	return firmware_version_len;
}

#else

int node_whisperer_information_hostname_parse(const uint8_t *ie_buf, size_t ie_len) {
	char hostname[255] = {};

	memcpy(hostname, ie_buf, ie_len);
	printf("Hostname: %s\n", hostname);
	return 0;
}

int node_whisperer_information_node_id_parse(const uint8_t *ie_buf, size_t ie_len) {
	/* We Print Node-ID on top */

	return 0;
}

int node_whisperer_information_batman_adv_parse(const uint8_t *ie_buf, size_t ie_len) {
	uint16_t *tmp;

	if (ie_len < 8)
		return -1;

	printf("VPN connected: %s\n", ie_buf[0] ? "Yes" : "No");
	printf("VPN-TQ: %d\n", ie_buf[1]);
	tmp = (uint16_t *)&ie_buf[2];
	printf("Originator count: %d\n", ntohs(*tmp));
	tmp = (uint16_t *)&ie_buf[4];
	printf("Neighbor count: %d\n", ntohs(*tmp));
	tmp = (uint16_t *)&ie_buf[6];
	printf("Client count: %d\n", ntohs(*tmp));

	return 0;
}

int node_whisperer_information_uptime_parse(const uint8_t *ie_buf, size_t ie_len) {
	uint32_t uptime_minutes;
	uint16_t days;
	uint8_t hours, minutes;

	if (ie_len < 4)
		return -1;

	memcpy(&uptime_minutes, ie_buf, sizeof(uptime_minutes));
	uptime_minutes = ntohl(uptime_minutes);

	minutes = uptime_minutes % 60;
	hours = (uptime_minutes / 60) % 24;
	days = uptime_minutes / (60 * 24);
	printf("Uptime: %d days, %d hours, %d minutes\n", days, hours, minutes);

	return 0;
}

int node_whisperer_information_site_code_parse(const uint8_t *ie_buf, size_t ie_len) {
	char *tmp;

	tmp = malloc(ie_len + 1);
	if (!tmp)
		return -1;

	memcpy(tmp, ie_buf, ie_len);
	tmp[ie_len] = '\0';

	printf("Site code: %s\n", tmp);
	free(tmp);

	return 0;
}

int node_whisperer_information_domain_parse(const uint8_t *ie_buf, size_t ie_len) {
	char *tmp;

	tmp = malloc(ie_len + 1);
	if (!tmp)
		return -1;

	memcpy(tmp, ie_buf, ie_len);
	tmp[ie_len] = '\0';

	printf("Domain: %s\n", tmp);
	free(tmp);

	return 0;
}

int node_whisperer_information_system_load_parse(const uint8_t *ie_buf, size_t ie_len) {
	double load;

	if (ie_len < 1)
		return -1;

	load = (double)ie_buf[0] / 0.1;
	printf("System load: %.1f\n", load);
	return 0;
}

int node_whisperer_information_firmware_version_parse(const uint8_t *ie_buf, size_t ie_len) {
	char *tmp;

	tmp = malloc(ie_len + 1);
	if (!tmp)
		return -1;

	memcpy(tmp, ie_buf, ie_len);
	tmp[ie_len] = '\0';

	printf("Firmware version: %s\n", tmp);
	free(tmp);

	return 0;
}

#endif


struct nw_information_source information_sources[] = {
	INFORMATION_SOURCE(hostname, 0),
	INFORMATION_SOURCE(node_id, 1),
	INFORMATION_SOURCE(uptime, 2),
	INFORMATION_SOURCE(site_code, 3),
	INFORMATION_SOURCE(domain, 4),
	INFORMATION_SOURCE(system_load, 5),
	INFORMATION_SOURCE(firmware_version, 6),
	INFORMATION_SOURCE(batman_adv, 20),
	{},
};
