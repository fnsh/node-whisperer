#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <stddef.h>
#include <errno.h>
#include <netlink/netlink.h>
#include <netlink/genl/genl.h>
#include <netlink/genl/ctrl.h>
#include <netlink/socket.h>
#include <linux/nl80211.h>
#include <net/if.h>

#include "ieee80211.h"
#include "information.h"
#include "log.h"
#include "util.h"

#define MAX_NODES 100
#define HOSTNAME_MAX_LEN

struct nl80211_sock {
	struct nl_sock *nls;
	int nl80211_id;
	int nlctrl_id;
};

struct trigger_results {
	int done;
	int aborted;
};

struct handler_args {
	const char *group;
	int id;
};

struct gluon_node {
	uint8_t node_id[6];
	char *information_elements;
	size_t information_elements_len;
};

struct scanned_gluon_nodes {
	struct gluon_node nodes[MAX_NODES];
	size_t len;
};

extern struct nw_information_source information_sources[];
static struct scanned_gluon_nodes scanned_nodes;

static int error_handler(struct sockaddr_nl *nla, struct nlmsgerr *err, void *arg)
{
	log_debug("error_handler() called. error=%d", err->error);
	int *ret = arg;
	*ret = err->error;
	return NL_STOP;
}


static int finish_handler(struct nl_msg *msg, void *arg)
{
	log_debug("finish_handler() called.");
	int *ret = arg;
	*ret = 0;
	return NL_SKIP;
}


static int ack_handler(struct nl_msg *msg, void *arg)
{
	log_debug("ack_handler() called.");
	int *ret = arg;
	*ret = 0;
	return NL_STOP;
}


static int no_seq_check(struct nl_msg *msg, void *arg)
{
	log_debug("no_seq_check() called.");
	return NL_OK;
}

int nl80211_socket_free(struct nl80211_sock *sock)
{
	if (sock->nls)
		nl_socket_free(sock->nls);
}

static int callback_trigger(struct nl_msg *msg, void *arg)
{
	struct genlmsghdr *gnlh = nlmsg_data(nlmsg_hdr(msg));
	struct trigger_results *results = arg;

	log_debug("callback_trigger() called. cmd=%d", gnlh->cmd);

	if (gnlh->cmd == NL80211_CMD_SCAN_ABORTED) {
		log_debug("Got NL80211_CMD_SCAN_ABORTED.");
		results->done = 1;
		results->aborted = 1;
	} else if (gnlh->cmd == NL80211_CMD_NEW_SCAN_RESULTS) {
		log_debug("Got NL80211_CMD_NEW_SCAN_RESULTS.");
		results->done = 1;
		results->aborted = 0;
	} else {
		log_debug("Got unknown cmd=%d.", gnlh->cmd);
	}

	return NL_SKIP;
}

static int family_handler(struct nl_msg *msg, void *arg)
{
	struct handler_args *grp = arg;
	struct nlattr *tb[CTRL_ATTR_MAX + 1];
	struct genlmsghdr *gnlh;
	struct nlattr *mcgrp;
	int rem_mcgrp;

	log_debug("family_handler() called.");

	gnlh = nlmsg_data(nlmsg_hdr(msg));

	nla_parse(tb, CTRL_ATTR_MAX, genlmsg_attrdata(gnlh, 0), genlmsg_attrlen(gnlh, 0), NULL);

	if (!tb[CTRL_ATTR_MCAST_GROUPS])
		return NL_SKIP;

	nla_for_each_nested(mcgrp, tb[CTRL_ATTR_MCAST_GROUPS], rem_mcgrp) {  // This is a loop.
		struct nlattr *tb_mcgrp[CTRL_ATTR_MCAST_GRP_MAX + 1];

		nla_parse(tb_mcgrp, CTRL_ATTR_MCAST_GRP_MAX, nla_data(mcgrp), nla_len(mcgrp), NULL);

		if (!tb_mcgrp[CTRL_ATTR_MCAST_GRP_NAME] || !tb_mcgrp[CTRL_ATTR_MCAST_GRP_ID])
			continue;
		if (strncmp(nla_data(tb_mcgrp[CTRL_ATTR_MCAST_GRP_NAME]), grp->group,
			nla_len(tb_mcgrp[CTRL_ATTR_MCAST_GRP_NAME]))) {
			continue;
		}

		grp->id = nla_get_u32(tb_mcgrp[CTRL_ATTR_MCAST_GRP_ID]);
		break;
	}

	return NL_SKIP;
}

int nl80211_socket_alloc(struct nl80211_sock *sock)
{
	int ret = 0;

	sock->nls = nl_socket_alloc();
	if (!sock->nls) {
		log_error("Failed to allocate netlink socket.");
		ret = -ENOMEM;
		goto out_free;
	}

	nl_socket_set_buffer_size(sock->nls, 8192, 8192);

	if (genl_connect(sock->nls)) {
		log_error("Failed to connect to netlink.");
		ret = -ENOLINK;
		goto out_free;
	}

	sock->nlctrl_id = genl_ctrl_resolve(sock->nls, "nlctrl");
	if (!sock->nlctrl_id) {
		log_error("nlctrl not found.");
		ret = -ENOENT;
		goto out_free;
	}
	sock->nl80211_id = genl_ctrl_resolve(sock->nls, "nl80211");
	if (sock->nl80211_id < 0) {
		log_error("nl80211 not found.");
		ret = -ENOENT;
		goto out_free;
	}

out_free:
	return ret;
}

int nl_get_multicast_id(struct nl80211_sock *sock, const char *family, const char *group)
{
	struct handler_args grp = { .group = group, .id = -ENOENT, };
	struct nl_msg *msg;
	struct nl_cb *cb;
	int ret, ctrlid;

	msg = nlmsg_alloc();
	if (!msg) {
		log_error("Failed to allocate netlink message.");
		ret = -ENOMEM;
		goto nla_put_failure;
	}

	cb = nl_cb_alloc(NL_CB_DEFAULT);
	if (!cb) {
		log_error("Failed to allocate callback message.");
		ret = -ENOMEM;
		goto nla_put_failure;
	}

	genlmsg_put(msg, 0, 0, sock->nlctrl_id, 0, 0, CTRL_CMD_GETFAMILY, 0);

	ret = 1;
	NLA_PUT_STRING(msg, CTRL_ATTR_FAMILY_NAME, family);

	nl_cb_err(cb, NL_CB_CUSTOM, error_handler, &ret);
	nl_cb_set(cb, NL_CB_ACK, NL_CB_CUSTOM, ack_handler, &ret);
	nl_cb_set(cb, NL_CB_VALID, NL_CB_CUSTOM, family_handler, &grp);

	ret = nl_send_auto_complete(sock->nls, msg);
	if (ret < 0) {
		log_debug("nl_send_auto_complete() returned %d (%s).", ret, nl_geterror(-ret));
		goto nla_put_failure;
	}

	ret = 1;
	while (ret > 0)
		nl_recvmsgs(sock->nls, cb);

	if (ret == 0) {
		ret = grp.id;
	}
	log_debug("Received multicast group ID: %d", ret);

nla_put_failure:
	nl_cb_put(cb);
	nlmsg_free(msg);
	return ret;
}

int nl80211_trigger_scan(struct nl80211_sock *wifi, int ifindex)
{
	struct trigger_results results = {};
	struct nl_msg *msg, *scan_ssid;
	struct nl_cb *cb;
	int ret, err, mcid;
	mcid = nl_get_multicast_id(wifi, "nl80211", "scan");
	if (mcid < 0) {
		log_error("Failed to get multicast ID.");
		ret = mcid;
		goto nla_put_failure;
	}

	ret = nl_socket_add_membership(wifi->nls, mcid);
	if (ret) {
		log_error("Failed to add membership.");
		goto nla_put_failure;
	}

	msg = nlmsg_alloc();
	if (!msg) {
		log_error("Failed to allocate netlink message.");
		ret = -ENOMEM;
		goto nla_put_failure;
	}

	scan_ssid = nlmsg_alloc();
	if (!scan_ssid) {
		log_error("Failed to allocate netlink message.");
		ret = -ENOMEM;
		goto nla_put_failure;
	}

	cb = nl_cb_alloc(NL_CB_DEFAULT);
	if (!cb) {
		log_error("Failed to allocate netlink callback.");
		ret = -ENOMEM;
		goto nla_put_failure;
	}

	/* Scan SSIDs */
	nla_put(scan_ssid, 1, 0, "");

	/* Root message */
	genlmsg_put(msg, 0, 0, wifi->nl80211_id, 0, 0, NL80211_CMD_TRIGGER_SCAN, 0);
	nla_put_nested(msg, NL80211_ATTR_SCAN_SSIDS, scan_ssid);
	NLA_PUT_U32(msg, NL80211_ATTR_IFINDEX, ifindex);


	err = -ENOENT;
	nl_cb_set(cb, NL_CB_VALID, NL_CB_CUSTOM, callback_trigger, &results);
	nl_cb_err(cb, NL_CB_CUSTOM, error_handler, &err);
	nl_cb_set(cb, NL_CB_FINISH, NL_CB_CUSTOM, finish_handler, &err);
	nl_cb_set(cb, NL_CB_ACK, NL_CB_CUSTOM, ack_handler, &err);
	nl_cb_set(cb, NL_CB_SEQ_CHECK, NL_CB_CUSTOM, no_seq_check, NULL);

	nl_send_auto_complete(wifi->nls, msg);

	while (err > 0)
		ret = nl_recvmsgs(wifi->nls, cb);

	if (ret < 0) {
		log_debug("ERROR: nl_recvmsgs() returned %d (%s).", ret, nl_geterror(-ret));
		goto nla_put_failure;
	}

	log_debug("Scan is started.");
	while (!results.done) {
		ret = nl_recvmsgs(wifi->nls, cb);
		if (ret < 0) {
			log_debug("ERROR: nl_recvmsgs() returned %d (%s).", ret, nl_geterror(-ret));
			goto nla_put_failure;
		}
	}

	if (results.aborted) {
		log_error("Kernel aborted scan.");
		ret = -1;
		goto nla_put_failure;
	}
	log_debug("Scan is done.");

nla_put_failure:
	if (cb)
		nl_cb_put(cb);
	if (msg)
		nlmsg_free(msg);
	if (scan_ssid)
		nlmsg_free(scan_ssid);
	nl_socket_drop_membership(wifi->nls, mcid);
	return ret;
}

static int monitor_gluon_node_parse_tlv_node_id(const uint8_t *tlv, size_t tlv_len, void *data)
{
	struct gluon_node *node = data;
	int ret;

	if (tlv[0] != 0x01) {
		log_debug("TLV is not a gluon node_id TLV.");
		return 0;
	}

	if (tlv[1] != 6) {
		log_error("TLV has invalid length for node-id.");
		return -EINVAL;
	}

	memcpy(node->node_id, tlv + 2, 6);
	log_debug("Found node_id: %02x:%02x:%02x:%02x:%02x:%02x", node->node_id[0], node->node_id[1], node->node_id[2], node->node_id[3], node->node_id[4], node->node_id[5]);

	return 0;
}

static int monitor_gluon_node_parse_ie(const uint8_t *ie, size_t ie_len, void *data)
{
	struct gluon_node *node = data;
	const uint8_t *gluon_tlv = ie + 6;
	size_t gluon_tlv_len = ie_len - 4 - 2;
	int ret;

	if (ie[0] != 0xdd) {
		log_debug("IE is not a vendor specific IE.");
		return 0;
	}

	/* Need header + OUI + vendor type before inspecting vendor-specific fields */
	if (ie_len < 6) {
		log_debug("Vendor IE too short to inspect OUI.");
		return 0;
	}

	if (memcmp(ie + 2, "\x00\x20\x91", 3) != 0) {
		log_debug("IE is not a gluon IE.");
		return 0;
	}

	if (ie[5] != 0x04) {
		log_debug("IE is not a gluon node IE.");
		return 0;
	}

	if (ie_len < 6 + 3) {
		log_error("Gluon node IE is too short.");
		return 0;
	}

	log_debug("Found gluon node IE length=%d", gluon_tlv_len);

	/* Validate TLVs. We can re-use the ieee80211 helper for this */
	ret = ieee80211_information_elements_validate(gluon_tlv, gluon_tlv_len);
	if (ret) {
		log_error("Failed to validate gluon TLV elements. ret=%d", ret);
		return ret;
	}

	node->information_elements = malloc(gluon_tlv_len);
	if (!node->information_elements) {
		log_error("Failed to allocate memory for gluon IE buffer.");
		ret = -ENOMEM;
	}
	memcpy(node->information_elements, gluon_tlv, gluon_tlv_len);
	node->information_elements_len = gluon_tlv_len;

	/* Only parse node-id now */
	ret = ieee80211_information_elements_iterate(gluon_tlv, gluon_tlv_len, &monitor_gluon_node_parse_tlv_node_id, node);
	if (ret) {
		log_error("Failed to parse gluon TLV elements. ret=%d", ret);
		goto out_free;
	}

out_free:
	if (ret) {
		free(node->information_elements);
		node->information_elements = NULL;
	}
	return ret;
}

static int monitor_gluon_node_add(const char *bssid,
				  const char *information_elements,
				  size_t information_elements_len)
{
	struct gluon_node *node;
	char mac_addr_string[20];
	char *ie_hex;
	size_t i;
	int ret;

	/* Format MAC-address */
	nw_format_mac_address_string(bssid, mac_addr_string);

	/* Print IE buffer */
	ie_hex = malloc(information_elements_len * 2 + 1);
	if (!ie_hex) {
		log_error("Failed to allocate memory for IE buffer.");
		return -ENOMEM;
	}

	nw_buffer_to_hexstring(information_elements, information_elements_len, ie_hex);
	log_debug("Processing scan-result bssid=%s ies=%s", mac_addr_string, ie_hex);
	free(ie_hex);

	ret = ieee80211_information_elements_validate(information_elements, information_elements_len);
	if (ret) {
		log_debug("Failed to validate information elements. ret=%d", ret);
		return ret;
	}

	if (scanned_nodes.len >= MAX_NODES) {
		log_error("Too many nodes scanned.");
		return -ENOMEM;
	}

	/* Add node */
	node = &scanned_nodes.nodes[scanned_nodes.len];
	ret = ieee80211_information_elements_iterate(information_elements, information_elements_len, monitor_gluon_node_parse_ie, node);
	if (ret) {
		log_error("Failed to parse information elements. ret=%d", ret);
		return ret;
	}

	/* Validate basic information */
	if (!node->information_elements) {
		log_debug("Node has no Gluon IE.");
		ret = 0;
		goto out_free;
	}

	/* Check if node_id was already seen on a prior node in the list */
	for (i = 0; i < scanned_nodes.len; i++) {
		if (memcmp(node->node_id, scanned_nodes.nodes[i].node_id, 6) == 0) {
			log_debug("Node-ID was already seen on a prior node.");
			ret = 0;
			goto out_free;
		}
	}

	if (!node->node_id[0] && !node->node_id[1] && !node->node_id[2] && !node->node_id[3] && !node->node_id[4] && !node->node_id[5]) {
		log_error("Node has no node_id.");
		ret = -EINVAL;
		goto out_free;
	}

	scanned_nodes.len++;
	return ret;

out_free:
	free(node->information_elements);
	memset(node, 0, sizeof(*node));
	return ret;
}

static int callback_dump(struct nl_msg *msg, void *arg)
{
	struct genlmsghdr *gnlh = nlmsg_data(nlmsg_hdr(msg));
	struct nlattr *tb[NL80211_ATTR_MAX + 1];
	struct nlattr *bss[NL80211_BSS_MAX + 1];
	static struct nla_policy bss_policy[NL80211_BSS_MAX + 1] = {
		[NL80211_BSS_FREQUENCY] = { .type = NLA_U32 },
		[NL80211_BSS_BSSID] = { },
		[NL80211_BSS_INFORMATION_ELEMENTS] = { },
	};

	log_debug("callback_dump() called.");

	nla_parse(tb, NL80211_ATTR_MAX, genlmsg_attrdata(gnlh, 0), genlmsg_attrlen(gnlh, 0), NULL);
	if (!tb[NL80211_ATTR_BSS]) {
		log_error("BSS is missing!");
		return NL_SKIP;
	}

	if (nla_parse_nested(bss, NL80211_BSS_MAX, tb[NL80211_ATTR_BSS], bss_policy)) {
		log_error("Failed to parse nested BSS attributes.");
		return NL_SKIP;
	}

	if (!bss[NL80211_BSS_BSSID] || !bss[NL80211_BSS_INFORMATION_ELEMENTS])
		return NL_SKIP;

	/* Add Node */
	monitor_gluon_node_add(nla_data(bss[NL80211_BSS_BSSID]), nla_data(bss[NL80211_BSS_INFORMATION_ELEMENTS]), nla_len(bss[NL80211_BSS_INFORMATION_ELEMENTS]));

	return NL_SKIP;
}

int nl80211_get_scan_results(struct nl80211_sock *wifi, int ifindex)
{
	struct nl_msg *msg;
	struct nl_cb *cb;
	int ret;

	msg = nlmsg_alloc();
	if (!msg) {
		log_error("Failed to allocate netlink message.");
		return -1;
	}

	genlmsg_put(msg, 0, 0, wifi->nl80211_id, 0, NLM_F_DUMP, NL80211_CMD_GET_SCAN, 0);
	nla_put_u32(msg, NL80211_ATTR_IFINDEX, ifindex);
	int err = 1;

	ret = nl_socket_modify_cb(wifi->nls, NL_CB_VALID, NL_CB_CUSTOM, callback_dump, NULL);
	if (ret < 0) {
		log_error("nl_socket_modify_cb() returned %d (%s).", ret, nl_geterror(-ret));
		goto out_free;
	}

	ret = nl_send_auto_complete(wifi->nls, msg);
	if (ret < 0) {
		log_error("nl_send_auto_complete() returned %d (%s).", ret, nl_geterror(-ret));
		goto out_free;
	}

	ret = nl_recvmsgs_default(wifi->nls);
	if (ret < 0) {
		log_error("nl_recvmsgs_default() returned %d (%s).", ret, nl_geterror(-ret));
		goto out_free;
	}

	log_debug("Scan results received.");

out_free:
	nlmsg_free(msg);

	return 0;
}

int monitor_gluon_node_print_elements(const uint8_t *tlv, size_t tlv_len, void *data)
{
	struct nw_information_source *information_source;
	struct gluon_node *node = data;
	char *ie_hex;
	int ret;
	int i;

	ie_hex = malloc(tlv_len * 2 + 1);
	if (!ie_hex) {
		log_error("Failed to allocate memory for IE buffer.");
		return -ENOMEM;
	}

	nw_buffer_to_hexstring(tlv, tlv_len, ie_hex);
	log_debug("Find parsing method ie=%s len=%d", ie_hex, tlv_len);
	free(ie_hex);

	/* Get print method for IE */
	for (i = 0; information_sources[i].name; i++) {
		information_source = &information_sources[i];
		log_debug("Check information source for type=%d name=%s", information_source->type, information_source->name);
		if (information_source->type == tlv[0]) {
			log_debug("Found information source for type=%d name=%s", tlv[0], information_source->name);

			/* Check length validity */
			if (tlv_len < 2 || tlv_len - 2 < tlv[1]) {
				log_error("IE has invalid length.");
				return -EINVAL;
			}

			ret = information_source->parse(&tlv[2], tlv[1]);
			if (ret) {
				log_error("Failed to parse information element. ret=%d", ret);
				return ret;
			}
		}
	}

	return 0;
}

int monitor_print_information(struct scanned_gluon_nodes *nodes)
{
	struct gluon_node *node;
	int ret;

	log_debug("Printing information for %d nodes.", nodes->len);


	for (size_t i = 0; i < nodes->len; i++) {
		node = &nodes->nodes[i];
		/* Print Node-ID */
		printf("Node-ID: %02x:%02x:%02x:%02x:%02x:%02x\n",
		       node->node_id[0], node->node_id[1], node->node_id[2],
		       node->node_id[3], node->node_id[4], node->node_id[5]);

		/* Print Information Elements */
		ret = ieee80211_information_elements_iterate(node->information_elements, node->information_elements_len, monitor_gluon_node_print_elements, node);

		/* Print newline */
		if (i < nodes->len - 1)
			printf("\n");
	}
	return 0;
}

int main(int argc, char *argv[])
{
	struct nl80211_sock wifi;
	int ifindex;
	int ret;

	if (argc < 2) {
		fprintf(stderr, "Usage: %s <interface> [debug]", argv[0]);
		return 1;
	}

	/* Clear list of scanned nodes */
	memset(&scanned_nodes, 0, sizeof(scanned_nodes));

	if (argc == 3 && atoi(argv[2]) == 1)
		log_set_level(LL_DEBUG);

	ifindex = if_nametoindex(argv[1]);
	log_debug("Interface index: %d", ifindex);
	ret = nl80211_socket_alloc(&wifi);
	if (ret) {
		log_error("Failed to allocate netlink socket.");
		goto out_free;
	}

	log_debug("Trigger scan");
	nl80211_trigger_scan(&wifi, ifindex);
	log_debug("Get scan results");
	nl80211_get_scan_results(&wifi, ifindex);

	/* Print node information */
	monitor_print_information(&scanned_nodes);
out_free:
	for (size_t i = 0; i < scanned_nodes.len; i++) {
		free(scanned_nodes.nodes[i].information_elements);
	}
	nl80211_socket_free(&wifi);
	return 0;
}
