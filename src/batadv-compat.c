// SPDX-License-Identifier: MIT
/* batman-adv helpers functions library
 *
 * Copyright (c) 2017, Sven Eckelmann <sven@narfation.org>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

/*
 * Copy of batadv_genl_get_algoname() and batadv_genl_get_algo() from Gluon
 * v2026.1 libbatadv. Remove once older Gluon releases are no longer supported.
 */

#include "batadv-compat.h"

#include <errno.h>
#include <stdbool.h>
#include <string.h>

struct batadv_algoname_opts {
	char *algoname;
	size_t algoname_len;
	bool found;
	struct batadv_nlquery_opts query_opts;
};

static int batadv_algoname_cb(struct nl_msg *msg, void *arg)
{
	struct nlattr *attrs[BATADV_ATTR_MAX + 1];
	struct batadv_nlquery_opts *query_opts = arg;
	struct batadv_algoname_opts *opts;
	struct nlmsghdr *nlh = nlmsg_hdr(msg);
	struct genlmsghdr *ghdr;

	opts = batadv_container_of(query_opts, struct batadv_algoname_opts,
			query_opts);

	if (!genlmsg_valid_hdr(nlh, 0))
		return NL_OK;

	ghdr = nlmsg_data(nlh);

	if (ghdr->cmd != BATADV_CMD_GET_MESH)
		return NL_OK;

	if (nla_parse(attrs, BATADV_ATTR_MAX, genlmsg_attrdata(ghdr, 0),
				genlmsg_len(ghdr), batadv_genl_policy))
		return NL_OK;

	if (!attrs[BATADV_ATTR_ALGO_NAME])
		return NL_OK;

	nla_strlcpy(opts->algoname, attrs[BATADV_ATTR_ALGO_NAME],
			opts->algoname_len);

	opts->found = true;
	return NL_OK;
}

/**
 * batadv_genl_get_algoname() - Query the routing algorithm name of a mesh
 * @mesh_iface: name of the batman-adv mesh interface
 * @algoname: buffer to store the algorithm name (e.g. "BATMAN_IV", "BATMAN_V")
 * @algoname_len: size of @algoname buffer
 *
 * Return: 0 on success or negative error value otherwise
 */
int batadv_genl_get_algoname(const char *mesh_iface, char *algoname,
		size_t algoname_len)
{
	struct batadv_algoname_opts opts = {
		.algoname = algoname,
		.algoname_len = algoname_len,
		.found = false,
		.query_opts = { .err = 0 },
	};
	int ret;

	ret = batadv_genl_query(mesh_iface, BATADV_CMD_GET_MESH,
			batadv_algoname_cb, 0, &opts.query_opts);
	if (ret < 0)
		return ret;

	if (!opts.found)
		return -EOPNOTSUPP;

	return 0;
}

/**
 * batadv_genl_get_algo() - Query the routing algorithm of a mesh as enum
 * @mesh_iface: name of the batman-adv mesh interface
 * @algo: pointer to store the algorithm (BATADV_ALGO_BATMAN_IV or
 *  BATADV_ALGO_BATMAN_V)
 *
 * Return: 0 on success or negative error value otherwise
 */
int batadv_genl_get_algo(const char *mesh_iface, enum batadv_algo *algo)
{
	char algoname[256];
	int ret;

	*algo = BATADV_ALGO_UNKNOWN;

	ret = batadv_genl_get_algoname(mesh_iface, algoname, sizeof(algoname));
	if (ret < 0)
		return ret;

	if (strcmp(algoname, "BATMAN_IV") == 0) {
		*algo = BATADV_ALGO_BATMAN_IV;
		return 0;
	} else if (strcmp(algoname, "BATMAN_V") == 0) {
		*algo = BATADV_ALGO_BATMAN_V;
		return 0;
	}

	return -EINVAL;
}
