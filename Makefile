include $(TOPDIR)/rules.mk

PKG_NAME:=luci-app-xgp-v3-screen
PKG_VERSION:=2.2
PKG_RELEASE:=8
PKG_BUILD_DEPENDS:=luci-base/host
PKG_LICENSE:=GPL-3.0-only

LVGL_COMMIT:=6ec2c47ceed51dd8872a48dd935aecc70fa01ffe
LVGL_ARCHIVE:=lvgl-$(LVGL_COMMIT).tar.gz

include $(INCLUDE_DIR)/package.mk
include $(INCLUDE_DIR)/cmake.mk

define Download/lvgl
	URL:=https://codeload.github.com/lvgl/lvgl/tar.gz
	URL_FILE:=$(LVGL_COMMIT)
	FILE:=$(LVGL_ARCHIVE)
	HASH:=ab7a7de211fb17d0ce0f0811aa9850104fb7d9dc9f726e13be39b9443667694a
endef
$(eval $(call Download,lvgl))

# Application package
define Package/$(PKG_NAME)
	SECTION:=utils
	CATEGORY:=Utilities
	TITLE:=NLnet XiGuaPi V3 TFT Screen
	DEPENDS:=@TARGET_rockchip_armv8 +python3 +libpthread +libstdcpp +luci-base
	URL:=https://github.com/zzzz0317/xgp-v3-screen
endef

define Package/$(PKG_NAME)/description
	NLnet XiGuaPi V3 TFT Screen with integrated LuCI Web UI control.
	Drives the GC9307 display through spidev and GPIO character devices.
endef

define Package/$(PKG_NAME)/conffiles
/etc/config/xgp_screen
endef

define Build/Prepare
	$(call Build/Prepare/Default)
	mkdir -p $(PKG_BUILD_DIR)
	$(CP) ./src/* $(PKG_BUILD_DIR)/
	mkdir -p $(PKG_BUILD_DIR)/lvgl
	tar -xzf $(DL_DIR)/$(LVGL_ARCHIVE) -C $(PKG_BUILD_DIR)/lvgl --strip-components=1
endef

define Package/$(PKG_NAME)/install
	$(INSTALL_DIR) $(1)/usr/zz
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/bin/zz_xgp_screen $(1)/usr/zz/zz_xgp_screen
	$(INSTALL_BIN) ./files/modem_info.py $(1)/usr/zz/modem_info.py
	$(INSTALL_BIN) ./files/network_devices.sh $(1)/usr/zz/network_devices.sh
	$(INSTALL_BIN) ./files/bind-panel.sh $(1)/usr/zz/bind-panel.sh
	$(INSTALL_DIR) $(1)/etc/init.d
	$(INSTALL_BIN) ./files/zz_xgp_screen.init $(1)/etc/init.d/zz_xgp_screen
	$(INSTALL_DIR) $(1)/etc/config
	$(INSTALL_CONF) ./files/xgp_screen.config $(1)/etc/config/xgp_screen
	# LuCI files
	$(INSTALL_DIR) $(1)/www/luci-static/resources/view
	$(INSTALL_DATA) ./htdocs/luci-static/resources/view/xgp_screen.js $(1)/www/luci-static/resources/view/
	$(INSTALL_DIR) $(1)/usr/share/luci/menu.d $(1)/usr/share/rpcd/acl.d $(1)/usr/share/rpcd/ucode
	$(INSTALL_DATA) ./root/usr/share/luci/menu.d/xgp-v3-screen.json $(1)/usr/share/luci/menu.d/
	$(INSTALL_DATA) ./root/usr/share/rpcd/acl.d/xgp-v3-screen.json $(1)/usr/share/rpcd/acl.d/
	$(INSTALL_DATA) ./root/usr/share/rpcd/ucode/xgp_screen.uc $(1)/usr/share/rpcd/ucode/
	$(INSTALL_DIR) $(1)/usr/lib/lua/luci/i18n
	$(STAGING_DIR_HOSTPKG)/bin/po2lmo ./po/zh_Hans/xgp-v3-screen.po $(1)/usr/lib/lua/luci/i18n/xgp-v3-screen.zh-cn.lmo
	# UCI defaults
	$(INSTALL_DIR) $(1)/etc/uci-defaults
	$(INSTALL_BIN) ./files/uci-defaults-xgp-screen $(1)/etc/uci-defaults/99-xgp-screen
endef

define Package/$(PKG_NAME)/postinst
#!/bin/sh
[ -n "$${IPKG_INSTROOT}" ] || /etc/init.d/rpcd reload
endef

$(eval $(call BuildPackage,$(PKG_NAME)))
