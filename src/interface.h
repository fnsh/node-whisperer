#pragma once

#include "node-whisperer.h"

struct nw_interface {
	struct list_head list;

	struct {
		int id;
		char *name;
		struct ubus_subscriber subscriber;
	} ubus;
};

int nw_interface_update(struct ubus_context *ctx, char *vendor_elements);

int nw_interface_remove(struct ubus_context *ctx,
				      struct nw_interface *iface);

int nw_interface_add(struct ubus_context *ctx, int id, const char *name);
