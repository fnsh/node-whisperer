/* SPDX-License-Identifier: MIT */

#pragma once

#include <batadv-genl.h>

/* Included in libbatadv since Gluon v2026.1 */
#ifndef HAVE_BATADV_GENL_GET_ALGO

#include <stddef.h>

enum batadv_algo {
	BATADV_ALGO_UNKNOWN = 0,
	BATADV_ALGO_BATMAN_IV,
	BATADV_ALGO_BATMAN_V,
};

int batadv_genl_get_algoname(const char *mesh_iface, char *algoname,
		size_t algoname_len);

int batadv_genl_get_algo(const char *mesh_iface, enum batadv_algo *algo);

#endif /* HAVE_BATADV_GENL_GET_ALGO */
