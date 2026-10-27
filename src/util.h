#pragma once

int nw_read_file(char *path, char **buf, size_t *len);
int nw_parse_mac_address_ascii(char *mac_address_ascii, uint8_t *mac_address);
int nw_format_mac_address_string(const uint8_t *mac_address, char *mac_address_string);
int nw_buffer_to_hexstring(const uint8_t *buf, size_t len, char *hexstring);
