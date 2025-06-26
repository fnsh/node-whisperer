#include <string.h>

#include "node-whisperer.h"
#include "daemon.h"

extern struct nw_information_source information_sources[];

static struct blob_buf b;

static struct blobmsg_policy set_interfaces_policy[] = {
	{ .name = "interfaces", .type = BLOBMSG_TYPE_ARRAY, },
};

static int nw_ubus_set_interfaces(struct ubus_context *ctx, struct ubus_object *obj,
				  struct ubus_request_data *req, const char *method,
				  struct blob_attr *msg)
{
	struct nw *nw = container_of(ctx, struct nw, ubus_ctx);
	struct nw_enabled_interface *iface;
	const char *ifname;
	struct blob_attr *cur;
	struct blob_attr *tb = NULL;
	size_t rem;

	/* Parse message */
	blobmsg_parse(set_interfaces_policy, 1, &tb, blob_data(msg), blob_len(msg));
	if (!tb)
		return UBUS_STATUS_INVALID_ARGUMENT;

	log_debug("Setting new interface list\n");

	/* Clear list */
	while (!list_empty(&nw->enabled_interfaces)) {
		log_debug("Clearing interface list\n");
		iface = list_first_entry(&nw->enabled_interfaces, struct nw_enabled_interface, list);
		list_del(&iface->list);
		free(iface->name);
		free(iface);
	}

	log_debug("Setting interface list\n");

	/* Add new interfaces */
	blobmsg_for_each_attr(cur, tb, rem) {
		if (blobmsg_type(cur) != BLOBMSG_TYPE_STRING)
			return UBUS_STATUS_INVALID_ARGUMENT;

		ifname = blobmsg_get_string(cur);
		if (!ifname)
			return UBUS_STATUS_INVALID_ARGUMENT;

		log_debug("Add interface %s\n", ifname);

		iface = calloc(1, sizeof(*iface));
		if (!iface)
			return UBUS_STATUS_UNKNOWN_ERROR;

		iface->name = strdup(ifname);
		if (!iface->name) {
			free(iface);
			return UBUS_STATUS_UNKNOWN_ERROR;
		}

		list_add_tail(&iface->list, &nw->enabled_interfaces);
	}

	return 0;
}


static struct blobmsg_policy source_toggle_arg[] = {
	{ .name = "information_sources", .type = BLOBMSG_TYPE_ARRAY, },
};

static struct nw_information_source *nw_information_source_get(const char *name)
{
	struct nw_information_source *source;

	for (source = information_sources; source->name; source++) {
		if (!strcmp(source->name, name))
			return source;
	}

	return NULL;
}

static int nw_ubus_information_source_set(const char *name, bool enabled)
{
	struct nw_information_source *source;

	source = nw_information_source_get(name);
	if (!source)
		return UBUS_STATUS_NOT_FOUND;

	source->enabled = enabled;
	return 0;
}

static int nw_ubus_enable_source(struct ubus_context *ctx, struct ubus_object *obj,
				 struct ubus_request_data *req, const char *method,
				 struct blob_attr *msg)
{
	struct blob_attr *source_name_blob, *cur;
	struct nw_information_source *source;
	char *source_name;
	size_t rem;

	blobmsg_parse(source_toggle_arg, 1, &source_name_blob, blob_data(msg), blob_len(msg));
	if (!source_name_blob)
		return UBUS_STATUS_INVALID_ARGUMENT;

	/* Disable all information sources first */
	for (source = information_sources; source->name; source++) {
		source->enabled = false;
	}

	/* Iterate over all sources given */
	blobmsg_for_each_attr(cur, source_name_blob, rem) {
		if (blobmsg_type(cur) != BLOBMSG_TYPE_STRING)
			return UBUS_STATUS_INVALID_ARGUMENT;

		source_name = blobmsg_get_string(cur);
		if (!source_name)
			return UBUS_STATUS_INVALID_ARGUMENT;

		nw_ubus_information_source_set(source_name, true);
	}

	return 0;
}

static int nw_ubus_get_sources(struct ubus_context *ctx, struct ubus_object *obj,
			       struct ubus_request_data *req, const char *method,
			       struct blob_attr *msg)
{
	struct nw_information_source *source;
	void *t, *a;

	blob_buf_init(&b, 0);

	a = blobmsg_open_array(&b, "information_sources");
	for (source = information_sources; source->name; source++) {
		t = blobmsg_open_table(&b, "");
		blobmsg_add_string(&b, "name", source->name);
		blobmsg_add_u8(&b, "enabled", source->enabled);
		blobmsg_close_table(&b, t);
	}
	blobmsg_close_array(&b, a);

	ubus_send_reply(ctx, req, b.head);
	blob_buf_free(&b);

	return 0;
}

static int nw_ubus_statistics(struct ubus_context *ctx, struct ubus_object *obj,
			      struct ubus_request_data *req, const char *method,
			      struct blob_attr *msg)
{
	struct nw *nw = container_of(ctx, struct nw, ubus_ctx);
	struct nw_information_source *source;
	void *t, *a;

	blob_buf_init(&b, 0);

	blobmsg_add_u64(&b, "update_count", nw->statistics.update_count);

	ubus_send_reply(ctx, req, b.head);
	blob_buf_free(&b);

	return 0;
}

static const struct ubus_method nw_methods[] = {
	UBUS_METHOD("set_interfaces", nw_ubus_set_interfaces, set_interfaces_policy),
	UBUS_METHOD("set_sources", nw_ubus_enable_source, source_toggle_arg),
	UBUS_METHOD_NOARG("get_sources", nw_ubus_get_sources),
	UBUS_METHOD_NOARG("statistics", nw_ubus_statistics),
};

static struct ubus_object_type nw_obj_type =
	UBUS_OBJECT_TYPE("node_whisperer", nw_methods);

struct ubus_object nw_obj = {
	.name = "node_whisperer",
	.type = &nw_obj_type,
	.methods = nw_methods,
	.n_methods = ARRAY_SIZE(nw_methods),
};

static void nw_ubus_event_handler(struct ubus_context *ctx,
						struct ubus_event_handler *ev,
						const char *type,
						struct blob_attr *msg)
{
	static const struct blobmsg_policy policy[2] = {
		{ .name = "id", .type = BLOBMSG_TYPE_INT32 },
		{ .name = "path", .type = BLOBMSG_TYPE_STRING },
	};
	struct blob_attr *tb[2];

	blobmsg_parse(policy, 2, tb, blob_data(msg), blob_len(msg));

	if (!tb[0] || !tb[1])
		return;

	nw_interface_add(ctx, blobmsg_get_u32(tb[0]), blobmsg_data(tb[1]));
}

static void nw_register_events(struct ubus_context *ctx)
{
	static struct ubus_event_handler handler = {
		.cb = nw_ubus_event_handler
	};

	ubus_register_event_handler(ctx, &handler, "ubus.object.add");
}

static void nw_ubus_list_cb(struct ubus_context *ctx,
					  struct ubus_object_data *obj,
					  void *priv)
{
	nw_interface_add(ctx, obj->id, obj->path);
}

void nw_ubus_init(struct ubus_context *ctx)
{
	ubus_add_object(ctx, &nw_obj);
	nw_register_events(ctx);
	ubus_lookup(ctx, "hostapd.*", nw_ubus_list_cb, NULL);
}