/* SPDX-FileCopyrightText: 2016-2019, Matthias Schiffer <mschiffer@universe-factory.net> */
/* SPDX-FileCopyrightText: 2023, David Bauer <mail@david-bauer.net> */
/* SPDX-License-Identifier: BSD-2-Clause */

#include <batadv-genl.h>

#include <netlink/netlink.h>
#include <netlink/genl/genl.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <net/if.h>

#include "batadv.h"

struct neigh_netlink_opts {
	int originator_count;
	struct batadv_nlquery_opts query_opts;
	struct nw_batadv_neighbor_stats *stats;
};

struct clients_netlink_opts {
	size_t clients;
	struct batadv_nlquery_opts query_opts;
};

static const enum batadv_nl_attrs parse_orig_list_mandatory[] = {
	BATADV_ATTR_ORIG_ADDRESS,
	BATADV_ATTR_NEIGH_ADDRESS,
	BATADV_ATTR_TQ,
	BATADV_ATTR_HARD_IFINDEX,
	BATADV_ATTR_LAST_SEEN_MSECS,
};

static const enum batadv_nl_attrs clients_mandatory[] = {
	BATADV_ATTR_TT_FLAGS,
	BATADV_ATTR_LAST_SEEN_MSECS,
};

static int parse_orig_list_netlink_cb(struct nl_msg *msg, void *arg)
{
	struct nlattr *attrs[BATADV_ATTR_MAX+1];
	struct nlmsghdr *nlh = nlmsg_hdr(msg);
	struct batadv_nlquery_opts *query_opts = arg;
	struct genlmsghdr *ghdr;
	struct neigh_netlink_opts *opts;
	char ifname[IF_NAMESIZE];
	uint32_t hardif;
	uint8_t tq;
	char *orig, *dest;

	opts = batadv_container_of(query_opts, struct neigh_netlink_opts,
				   query_opts);

	if (!genlmsg_valid_hdr(nlh, 0))
		return NL_OK;

	ghdr = nlmsg_data(nlh);

	if (ghdr->cmd != BATADV_CMD_GET_ORIGINATORS)
		return NL_OK;

	if (nla_parse(attrs, BATADV_ATTR_MAX, genlmsg_attrdata(ghdr, 0),
		      genlmsg_len(ghdr), batadv_genl_policy))
		return NL_OK;

	if (batadv_genl_missing_attrs(attrs, parse_orig_list_mandatory,
				      BATADV_ARRAY_SIZE(parse_orig_list_mandatory)))
		return NL_OK;

	hardif = nla_get_u32(attrs[BATADV_ATTR_HARD_IFINDEX]);
	orig = nla_data(attrs[BATADV_ATTR_ORIG_ADDRESS]);
	dest = nla_data(attrs[BATADV_ATTR_NEIGH_ADDRESS]);

	if (if_indextoname(hardif, ifname) == NULL)
		return NL_OK;

	opts->stats->originator_count++;
	if (memcmp(orig, dest, 6) != 0)
		return NL_OK;

	opts->stats->neighbor_count++;
	if (!strncmp(ifname, "mesh-vpn", sizeof("mesh-vpn"))) {
		opts->stats->vpn.tq = nla_get_u8(attrs[BATADV_ATTR_TQ]);
		opts->stats->vpn.connected = 1;
	}
	return NL_OK;
}

int nw_get_batadv_neighbor_stats(struct nw_batadv_neighbor_stats *stats) {
	struct neigh_netlink_opts opts = {
		.query_opts = {
			.err = 0,
		},
		.stats = stats,
	};
	int ret;

	ret = batadv_genl_query("bat0", BATADV_CMD_GET_ORIGINATORS,
				parse_orig_list_netlink_cb, NLM_F_DUMP,
				&opts.query_opts);
	if (ret < 0) {
		return -1;
	}

	return 0;
}


static int parse_clients_list_netlink_cb(struct nl_msg *msg, void *arg)
{
	struct nlattr *attrs[BATADV_ATTR_MAX+1];
	struct nlmsghdr *nlh = nlmsg_hdr(msg);
	struct batadv_nlquery_opts *query_opts = arg;
	struct genlmsghdr *ghdr;
	struct clients_netlink_opts *opts;
	uint32_t flags, lastseen;

	opts = batadv_container_of(query_opts, struct clients_netlink_opts,
			query_opts);

	if (!genlmsg_valid_hdr(nlh, 0))
		return NL_OK;

	ghdr = nlmsg_data(nlh);

	if (ghdr->cmd != BATADV_CMD_GET_TRANSTABLE_LOCAL)
		return NL_OK;

	if (nla_parse(attrs, BATADV_ATTR_MAX, genlmsg_attrdata(ghdr, 0),
				genlmsg_len(ghdr), batadv_genl_policy))
		return NL_OK;

	if (batadv_genl_missing_attrs(attrs, clients_mandatory,
				BATADV_ARRAY_SIZE(clients_mandatory)))
		return NL_OK;

	flags = nla_get_u32(attrs[BATADV_ATTR_TT_FLAGS]);

	if (flags & (BATADV_TT_CLIENT_NOPURGE))
		return NL_OK;

	lastseen = nla_get_u32(attrs[BATADV_ATTR_LAST_SEEN_MSECS]);
	if (lastseen > (60 * 1000))
		return NL_OK;

	opts->clients++;

	return NL_OK;
}

int nw_get_batadv_clients() {
	struct clients_netlink_opts opts = {
		.clients = 0,
		.query_opts = {
			.err = 0,
		},
	};
	int ret;

	ret = batadv_genl_query("bat0", BATADV_CMD_GET_TRANSTABLE_LOCAL,
							parse_clients_list_netlink_cb, NLM_F_DUMP,
							&opts.query_opts);
	if (ret < 0) {
		return -1;
	}

	return opts.clients;
}
