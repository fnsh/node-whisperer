#include "node-whisperer.h"
#include "daemon.h"

#define UBUS_HOSTAPD_PREFIX "hostapd."

static LIST_HEAD(nw_interfaces);
static struct blob_buf b;

static int nw_interface_handle_event(struct ubus_context *ctx,
						   struct ubus_object *obj,
						   struct ubus_request_data *req,
						   const char *method,
						   struct blob_attr *msg)
{
	/* Nothing to handle, we are here for the removal */
	return 0;
}

static void nw_interface_handle_remove(struct ubus_context *ctx,
						     struct ubus_subscriber *s,
						     uint32_t id)
{
	struct nw_interface *iface;
	iface = container_of(s, struct nw_interface, ubus.subscriber);

	nw_interface_remove(ctx, iface);
}

static int nw_interface_enabled(struct nw *instance, const char *name)
{
	struct nw_enabled_interface *iface;
	const char *hostapd_name;

	/* Always enabled when list of enabled interfaces empty */
	if (list_empty(&instance->enabled_interfaces)) {
		return 1;
	}

	if (strlen(name) <= strlen(UBUS_HOSTAPD_PREFIX)) {
		return 0;
	}

	hostapd_name = (char *)name + strlen(UBUS_HOSTAPD_PREFIX);

	list_for_each_entry(iface, &instance->enabled_interfaces, list) {
		if (!strcmp(hostapd_name, iface->name)) {
			return 1;
		}
	}

	return 0;
}

int nw_interface_update(struct ubus_context *ctx, char *vendor_elements)
{
	struct nw *instance = container_of(ctx, struct nw, ubus_ctx);
	struct nw_interface *iface;
	int ret;

	list_for_each_entry(iface, &nw_interfaces, list) {
		blob_buf_init(&b, 0);
		blobmsg_add_string(&b, "vendor_elements", vendor_elements);

		if (!nw_interface_enabled(instance, iface->ubus.name)) {
			continue;
		}

		log_debug("Sending vendor elements to id=%d name=%s", iface->ubus.id, iface->ubus.name);
		ret = ubus_invoke(ctx, iface->ubus.id, "set_vendor_elements", b.head, NULL, NULL, 1000);
		if (ret == UBUS_STATUS_TIMEOUT) {
			log_info("Timeout sending vendor elements to id=%d name=%s", iface->ubus.id, iface->ubus.name);
			continue;
		}
		if (ret) {
			log_error("Failed to send vendor elements to id=%d name=%s error=%s", iface->ubus.id, iface->ubus.name, ubus_strerror(ret));

			/* Delete element */
			blob_buf_init(&b, 0);
			blobmsg_add_string(&b, "vendor_elements", "");

			ret = ubus_invoke(ctx, iface->ubus.id, "set_vendor_elements", b.head, NULL, NULL, 1000);
			if (ret) {
				log_error("Failed to reset vendor elements for id=%d name=%s error=%s", iface->ubus.id, iface->ubus.name, ubus_strerror(ret));
			}
		}
	}

	return 0;
}

int nw_interface_remove(struct ubus_context *ctx,
				      struct nw_interface *iface)
{
	log_info("Removing interface %s", iface->ubus.name);

	ubus_unregister_subscriber(ctx, &iface->ubus.subscriber);
	list_del(&iface->list);
	free(iface->ubus.name);
	free(iface);
	return 0;
}

int nw_interface_add(struct ubus_context *ctx, int id, const char *name)
{
	struct nw_interface *iface;

	iface = calloc(sizeof(*iface), 1);
	if (!iface) {
		log_error("Failed to allocate memory for interface %s", name);
		return -ENOMEM;
	}

	iface->ubus.id = id;
	iface->ubus.name = strdup(name);
	if (!iface->ubus.name) {
		log_error("Failed to allocate memory for interface %s", name);
		free(iface);
		return -ENOMEM;
	}

	/* Add to node-list */
	INIT_LIST_HEAD(&iface->list);
	list_add(&iface->list, &nw_interfaces);

	/* Subscribe to node removal */
	iface->ubus.subscriber.cb = nw_interface_handle_event;
	iface->ubus.subscriber.remove_cb = nw_interface_handle_remove;
	ubus_register_subscriber(ctx, &iface->ubus.subscriber);
	ubus_subscribe(ctx, &iface->ubus.subscriber, iface->ubus.id);

	log_info("Registered interface %s", iface->ubus.name);

	return 0;
}
