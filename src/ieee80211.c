#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <stddef.h>
#include <errno.h>

#include "log.h"

int ieee80211_information_elements_iterate(uint8_t *ie_buf, size_t ie_buf_len, int (*cb)(uint8_t *ie, size_t ie_len, void *data), void *data) {
	size_t ie_remaining, ie_len;
	uint8_t *ie;

	ie = ie_buf;
	ie_remaining = ie_buf_len;
	while (ie_remaining > 0) {
		/* Check if we have at least 2 bytes */
		if (ie_remaining < 2) {
			return -1;
		}

		/* Validate length of the current IE */
		ie_len = ie[1];

		log_debug("Process Information Element element=%d length=%d", ie[0], ie_len);
		if (ie_len > ie_remaining - 2) {
			log_debug("Invalid Information Element length %d > %d", ie_len, ie_remaining - 2);
			return -1;
		}

		if (ie_len == 0 || ie_len == 0xff) {
			log_debug("Invalid Information Element length %d", ie_len);
			return -1;
		}

		if (cb) {
			log_debug("Hand over to callback");
			if (cb(ie, ie_len + 2, data) != 0)
				return -1;
		}

		ie += ie_len + 2;
		ie_remaining -= ie_len + 2;
	}
	return 0;
}

int ieee80211_information_elements_validate(uint8_t *ie_buf, size_t ie_buf_len) {
	return ieee80211_information_elements_iterate(ie_buf, ie_buf_len, NULL, NULL);
}
