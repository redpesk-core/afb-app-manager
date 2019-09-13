This directory contains the source of the AGL application framework.


Summary of source files
=======================

afm-udb.[ch]
-----------

Builds in memory the list of the applications.
Provide application database.

afm-urun.[ch]
------------

Manages the live of running applications using systemd:
starts it, stop it, continue it, terminates it, list it.

afm-binding.c
-------------

The binding that implements afm-system-daemon: the service
that installs, uninstalls, runs, list applications and services.

afm-user-daemon.[ch]
--------------------

Legacy daemon that at the moment just bridge DBUS calls to
the true API. To be removed for FF (Funky Flounder).

widget configuration
--------------------
(wgt.c wgt-config.c wgt-info.c wgt-json.c wgt-strings.c)

Access to files of a widget.

Implements the mechanism for seeking for files using the locale settings. 

Fact that it seeks for locale if done until EE (Electric Eel version of AGL).
Locale management to be changed for FF (Funky Flounder).

utilities
---------
(mustach.c secmgr-wrap.c utils-dir.c utils-file.c utils-jbus.c utils-json.c utils-systemd.c verbose.c wrap-json.c)

These files provide utility features.

utils-jbus is only used by afm-user-daemon. It should be removed for FF.

widget package management
-------------------------
(wgtpkg-base64.c
wgtpkg-certs.c
wgtpkg-digsig.c
wgtpkg-files.c
main-wgtpkg-info.c
wgtpkg-install.c
main-wgtpkg-instal.c
wgtpkg-mustach.c
main-wgtpkg-pack.c
wgtpkg-permissions.c
main-wgtpkg-sign.c
wgtpkg-uninstall.c
wgtpkg-unit.c
wgtpkg-workdir.c
wgtpkg-xmlsec.c
wgtpkg-zip.c)

Components for handling widgets as packages of data.
