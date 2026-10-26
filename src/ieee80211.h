#pragma once

#include <stdlib.h>
#include <stdint.h>

int ieee80211_information_elements_iterate(const uint8_t *ie_buf, size_t ie_buf_len, int (*cb)(const uint8_t *ie, size_t ie_len, void *data), void *data);

int ieee80211_information_elements_validate(const uint8_t *ie_buf, size_t ie_buf_len);
