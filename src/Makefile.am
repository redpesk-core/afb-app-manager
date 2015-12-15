bin_PROGRAMS = \
	wgtpkg-installer \
	wgtpkg-pack \
	wgtpkg-sign \
	wgtpkg-info \
	appfwk

OTHERSRCS = \
	utils-dir.c \
	verbose.c

WGTPKGSRCS = \
	wgtpkg-base64.c \
	wgtpkg-certs.c \
	wgtpkg-digsig.c \
	wgtpkg-files.c \
	wgtpkg-install.c \
	wgtpkg-permissions.c \
	wgtpkg-workdir.c \
	wgtpkg-xmlsec.c \
	wgtpkg-zip.c

WGTSRCS = \
	wgt-config.c \
	wgt-info.c \
	wgt.c

SECWRP = \
	secmgr-wrap.c

APPFWK = \
	appfwk.c


AM_CFLAGS  = -Wall -Wno-pointer-sign
AM_CFLAGS += -ffunction-sections -fdata-sections

fwk_name = aglfwk
fwk_confdir = $(sysconfdir)/$(fwk_name)
fwk_datadir = $(datadir)/$(fwk_name)
fwk_appdir = $(fwk_datadir)/applications
fwk_icondir = $(fwk_datadir)/icons
fwk_prefix = urn:agl:
fwk_prefix_permission = $(fwk_prefix)perm:
fwk_prefix_plugin = $(fwk_prefix)plugin:

AM_CFLAGS += -DFWK_CONFIG_DIR=\"$(fwk_confdir)\"
AM_CFLAGS += -DFWK_PREFIX_PERMISSION=\"$(fwk_prefix_permission)\"
AM_CFLAGS += -DFWK_PREFIX_PLUGIN=\"$(fwk_prefix_plugin)\"
AM_CFLAGS += -DFWK_ICON_DIR=\"$(fwk_icondir)\"
AM_CFLAGS += -DFWK_APP_DIR=\"$(fwk_appdir)\"

AM_LDFLAGS = -Wl,--gc-sections

AM_CFLAGS += \
	${DBUS_CFLAGS} \
	${JSON_CFLAGS} \
	${OPENSSL_CFLAGS} \
	${XML2_CFLAGS} \
	${XMLSEC_CFLAGS} \
	${ZIP_CFLAGS}

LDADD = \
	${DBUS_LIBS} \
	${JSON_LIBS} \
	${OPENSSL_LIBS} \
	${XML2_LIBS} \
	${XMLSEC_LIBS} \
	${ZIP_LIBS}

# remove (or comment) following line to really use simulated components
AM_CFLAGS += -Isimulation


wgtpkg_sign_SOURCES = wgtpkg-sign.c ${WGTPKGSRCS} ${OTHERSRCS}

wgtpkg_pack_SOURCES = wgtpkg-pack.c ${WGTPKGSRCS} ${OTHERSRCS}

wgtpkg_installer_SOURCES = wgtpkg-installer.c ${WGTPKGSRCS} ${WGTSRCS} ${SECWRP} ${OTHERSRCS}

wgtpkg_info_SOURCES = wgtpkg-info.c ${WGTPKGSRCS} ${WGTSRCS} ${OTHERSRCS}

appfwk_SOURCES = ${APPFWK} ${WGTSRCS} ${OTHERSRCS}


