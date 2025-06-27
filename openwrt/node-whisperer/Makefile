include $(TOPDIR)/rules.mk

PKG_NAME:=node-whisperer
PKG_VERSION:=2
PKG_RELEASE:=2

include $(INCLUDE_DIR)/package.mk
include $(INCLUDE_DIR)/package-defaults.mk

define Build/Prepare
	mkdir -p $(PKG_BUILD_DIR)
	cp -r $(CURDIR)/../../src $(PKG_BUILD_DIR)/
endef

MAKE_PATH:=src
MAKE_VARS += \
        NL_LIB="libnl-tiny" \
        NL_GENL_LIB="libnl-tiny"

define Package/node-whisperer
  SECTION:=net
  CATEGORY:=Network
  DEPENDS:=+libubox +libubus +libblobmsg-json +libnl-tiny +libbatadv +libgluonutil
  TITLE:=Daemon for Gluon diagnostic using 802.11 beacon frames
endef

define Package/node-whisperer/conffiles
/etc/config/node-whisperer
endef

define Package/node-whisperer/install
	$(INSTALL_DIR) $(1)/usr/bin $(1)/etc/init.d $(1)/etc/config $(1)/lib/gluon/upgrade
	$(CP) $(PKG_BUILD_DIR)/src/node-whisperer $(1)/usr/bin
	$(INSTALL_BIN) ./files/node-whisperer.init $(1)/etc/init.d/node-whisperer
	$(INSTALL_BIN) ./files/node-whisperer.upgrade.lua $(1)/lib/gluon/upgrade/150-node-whisperer
	$(CP) ./files/node-whisperer.uci $(1)/etc/config/node-whisperer
endef

$(eval $(call BuildPackage,node-whisperer))
