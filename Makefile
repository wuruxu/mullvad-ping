include $(TOPDIR)/rules.mk

PKG_NAME:=mullvad-ping
PKG_VERSION:=1.0
PKG_RELEASE:=1
PKG_BUILD_DIR:=$(BUILD_DIR)/$(PKG_NAME)

include $(INCLUDE_DIR)/package.mk

define Package/mullvad-ping
	SECTION:=utils
	CATEGORY:=Utilities
	TITLE:=mullvad.net ping tools
	DEPENDS:=+libzip
	MAINTAINER:=wuruxu <wrxzzj@gmail.com>
endef

define Package/mullvad-ping/description
	This is a simple ping tool for mullvad.net.
endef

define Build/Prepare
	mkdir -p $(PKG_BUILD_DIR)
	$(CP) ./src/* $(PKG_BUILD_DIR)/
endef

define Build/Configure
	# No configuration needed for this simple program
endef

define Build/Compile
	$(TARGET_CC) $(TARGET_CFLAGS) -o $(PKG_BUILD_DIR)/mullvad-ping $(PKG_BUILD_DIR)/mullvad-ping.c -lzip
endef

define Package/mullvad-ping/install
	$(INSTALL_DIR) $(1)/usr/bin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/mullvad-ping $(1)/usr/bin/
endef

$(eval $(call BuildPackage,mullvad-ping))
