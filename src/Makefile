CC ?= gcc
CFLAGS_common ?= -Wall -Wextra -Wno-unused-parameter -Werror -std=c99 -g
LDFLAGS_common ?=

PKG_CONFIG ?= pkg-config

ifndef NL_LIB
NL_LIB:=libnl-3.0
endif

ifndef NL_GENL_LIB
NL_GENL_LIB:=libnl-genl-3.0
endif

CFLAGS_nl:=$(shell pkg-config --cflags $(NL_LIB))
LDFLAGS_nl:=$(shell pkg-config --libs $(NL_LIB))

CFLAGS_genl:=$(shell pkg-config --cflags $(NL_GENL_LIB))
LDFLAGS_genl:=$(shell pkg-config --libs $(NL_GENL_LIB))

CFLAGS_batadv:=$(shell pkg-config --cflags libbatadv)
LDFLAGS_batadv:=$(shell pkg-config --libs libbatadv)

CFLAGS_json:=$(shell pkg-config --cflags json-c)
LDFLAGS_json:=$(shell pkg-config --libs json-c)


LDFLAGS_DAEMON:=-lubox -lubus -lblobmsg_json -lgluonutil -luci -lm $(LDFLAGS_nl) $(LDFLAGS_batadv) $(LDFLAGS_json)
CFLAGS_DAEMON:=$(CFLAGS_nl) $(CFLAGS_batadv) $(CFLAGS_json)

# Gluon releases before v2026.1 lack batadv_genl_get_algo() in libbatadv,
# so build the bundled copy for them
HAVE_BATADV_GENL_GET_ALGO:=$(shell echo 'int (*f)(const char *, enum batadv_algo *) = batadv_genl_get_algo;' | \
	$(CC) $(CFLAGS_DAEMON) $(CFLAGS) -D_GNU_SOURCE -include batadv-genl.h -fsyntax-only -x c - >/dev/null 2>&1 && echo y)
ifeq ($(HAVE_BATADV_GENL_GET_ALGO),y)
CFLAGS_DAEMON+=-DHAVE_BATADV_GENL_GET_ALGO
else
SRC_BATADV_COMPAT:=batadv-compat.c
endif

LDFLAGS_MONITOR:=$(LDFLAGS_genl)
CFLAGS_MONITOR:=$(CFLAGS_genl) -DBUILD_MONITOR

all: node-whisperer node-monitor

node-whisperer:
	$(CC) $(CFLAGS_common) $(CFLAGS_DAEMON) $(CFLAGS) -D_GNU_SOURCE -o node-whisperer daemon.c util.c information.c log.c ubus.c interface.c batadv.c $(SRC_BATADV_COMPAT) $(LDFLAGS_common) $(LDFLAGS) $(LDFLAGS_DAEMON)

node-monitor:
	$(CC) $(CFLAGS_common) $(CFLAGS_MONITOR) $(CFLAGS) -D_GNU_SOURCE -o node-monitor monitor.c log.c util.c ieee80211.c information.c $(LDFLAGS_common) $(LDFLAGS) $(LDFLAGS_MONITOR)
