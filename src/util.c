#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include <string.h> // Add missing import

int nw_read_file(char *path, char **buf, size_t *len) {
	FILE *f = fopen(path, "rb");
	if (!f) {
		return -1;
	}
	fseek(f, 0, SEEK_END);
	*len = ftell(f);
	fseek(f, 0, SEEK_SET);
	*buf = malloc(*len);
	if (!*buf) {
		fclose(f);
		return -1;
	}
	if (fread(*buf, 1, *len, f) != *len) {
		free(*buf);
		fclose(f);
		return -1;
	}
	fclose(f);
	return 0;
}

/* Parse MAC address stored in xx:xx:xx:xx:xx:xx Format*/
int nw_parse_mac_address_ascii(char *mac_address_ascii, uint8_t *mac_address) {
	int values[6];
	int i;

	if (strlen(mac_address_ascii) < 18) {
		return -1;
	}

	if(sscanf(mac_address_ascii, "%x:%x:%x:%x:%x:%x%*c",
		  &values[0], &values[1], &values[2],
		  &values[3], &values[4], &values[5] ) != 6) {
		return -1;
	}

	for (i = 0; i < 6; i++) {
		mac_address[i] = (uint8_t) values[i];
	}

	return 0;
}

int nw_format_mac_address_string(const uint8_t *mac_address, char *mac_address_string) {
	sprintf(mac_address_string, "%02x:%02x:%02x:%02x:%02x:%02x",
		mac_address[0], mac_address[1], mac_address[2],
		mac_address[3], mac_address[4], mac_address[5]);
	return 0;
}

int nw_buffer_to_hexstring(const uint8_t *buf, size_t len, char *hexstring) {
	for (size_t i = 0; i < len; i++) {
		sprintf(&hexstring[i * 2], "%02x", buf[i]);
	}
	return 0;
}
