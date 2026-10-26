#pragma once

#include "node-whisperer.h"

struct nw_enabled_interface {
	struct list_head list;
	char *name;
};

struct nw {
	struct ubus_context ubus_ctx;
	struct uloop_timeout update_timeout;
	struct list_head enabled_interfaces;

	struct {
		uint8_t *buf;
		size_t len;  /* Content Length*/
		size_t size; /* Buffer Size */
	} output;

	struct {
		uint64_t update_count;
	} statistics;
};
