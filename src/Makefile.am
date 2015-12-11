bin_PROGRAMS = wgtpkg-installer wgtpkg-pack wgtpkg-sign wgtpkg-info appfwk

OTHERSRCS = \
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


#pkgsysconfdir = $(sysconfdir)
pkgsysconfdir = .
#deffwdir = $(datadir)/af
deffwdir = ./af
defappdir = $(deffwdir)/applications
deficondir = $(deffwdir)/icons

AM_CFLAGS  = -Wall -Wno-pointer-sign
AM_CFLAGS += -ffunction-sections -fdata-sections
AM_CFLAGS += ${ZIP_CFLAGS} ${XML2_CFLAGS} ${OPENSSL_CFLAGS} ${XMLSEC_CFLAGS}

AM_CFLAGS += -Isimulation

AM_CFLAGS += -DPKGSYSCONFDIR=\"$(pkgsysconfdir)\"
AM_CFLAGS += -DPREFIXPERMISSION=\"urn:agl-perm:\"
AM_CFLAGS += -DICONDESTDIR=\"$(deficondir)\"
AM_CFLAGS += -DAPPDEFDIR=\"$(defappdir)\"


AM_LDFLAGS = -Wl,--gc-sections

LDADD = ${ZIP_LIBS} ${XML2_LIBS} ${OPENSSL_LIBS} ${XMLSEC_LIBS}

wgtpkg_sign_SOURCES = wgtpkg-sign.c ${WGTPKGSRCS} ${OTHERSRCS}

wgtpkg_pack_SOURCES = wgtpkg-pack.c ${WGTPKGSRCS} ${OTHERSRCS}

wgtpkg_installer_SOURCES = wgtpkg-installer.c ${WGTPKGSRCS} ${WGTSRCS} ${SECWRP} ${OTHERSRCS}

wgtpkg_info_SOURCES = wgtpkg-info.c ${WGTPKGSRCS} ${WGTSRCS} ${OTHERSRCS}

appfwk_SOURCES = ${APPFWK} ${OTHERSRCS}

