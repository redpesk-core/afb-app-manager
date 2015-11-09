.PHONY: all

all: wgtpkg-install wgtpkg-pack wgtpkg-sign

O = -DPREDIR='""'

INCS = wgtpkg.h

COMSRCS = \
	wgtpkg-base64.c \
	wgtpkg-certs.c \
	wgtpkg-digsig.c \
	wgtpkg-files.c \
	wgtpkg-workdir.c \
	wgtpkg-xmlsec.c \
	wgtpkg-zip.c


INSTALLSRCS = wgtpkg-install.c $(COMSRCS)

PACKSRCS = wgtpkg-install.c $(COMSRCS)

XMLSECOPT = $(shell pkg-config --cflags --libs xmlsec1)

wgtpkg-%: wgtpkg-%.c $(COMSRCS) $(INCS)
	gcc $O -g -o wgtpkg-$* wgtpkg-$*.c $(COMSRCS) -lzip -lxml2 -lcrypto $(XMLSECOPT) -Wall -Wno-pointer-sign 



