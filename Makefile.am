bin_PROGRAMS = wgtpkg-install wgtpkg-pack wgtpkg-sign

COMMONSRCS = \
	wgtpkg-base64.c \
	wgtpkg-certs.c \
	wgtpkg-digsig.c \
	wgtpkg-files.c \
	wgtpkg-verbose.c \
	wgtpkg-workdir.c \
	wgtpkg-xmlsec.c \
	wgtpkg-zip.c

WGTSRCS = \
	wgt-config-xml.c \
	wgt-locales.c \
	wgt-strings.c

AM_CFLAGS  = -Wall -Wno-pointer-sign
AM_CFLAGS += -ffunction-sections -fdata-sections
AM_CFLAGS += ${ZIP_CFLAGS} ${XML2_CFLAGS} ${OPENSSL_CFLAGS} ${XMLSEC_CFLAGS}

AM_LDFLAGS = -Wl,--gc-sections

LDADD = ${ZIP_LIBS} ${XML2_LIBS} ${OPENSSL_LIBS} ${XMLSEC_LIBS}

wgtpkg_install_SOURCES = wgtpkg-install.c ${WGTSRCS} ${COMMONSRCS}

wgtpkg_sign_SOURCES = wgtpkg-sign.c ${COMMONSRCS}

wgtpkg_pack_SOURCES = wgtpkg-pack.c ${COMMONSRCS}

