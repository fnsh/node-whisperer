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


LDFLAGS:=-lubox -lubus -lblobmsg_json -lgluonutil -luci $(LDFLAGS_nl) $(LDFLAGS_batadv) $(LDFLAGS_json)
CFLAGS:=$(CFLAGS_nl) $(CFLAGS_batadv) $(CFLAGS_json)

LDFLAGS_MONITOR:=$(LDFLAGS_genl) -DBUILD_MONITOR
CFLAGS_MONITOR:=$(CFLAGS_genl)

all: node-whisperer node-monitor

node-whisperer:
	$(CC) $(CFLAGS_common) $(CFLAGS) -D_GNU_SOURCE -o node-whisperer daemon.c util.c information.c log.c ubus.c interface.c batadv.c $(LDFLAGS_common) $(LDFLAGS)

node-monitor:
	$(CC) $(CFLAGS_common) $(CFLAGS_MONITOR) -D_GNU_SOURCE -o node-monitor monitor.c log.c util.c ieee80211.c information.c $(LDFLAGS_common) $(LDFLAGS_MONITOR)
