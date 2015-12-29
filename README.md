# afm-main

## Overview

This repository is named **afm-main** because
it stands for **AGL Framework Master - Main**.

It contains programs and services to create widgets,
to install widgets, to run widgets.

## How to compile?

This project uses CMAKE and C compiler suite to be compiled.

### Dependencies

This package requires the following libraries or modules:

- ***libxml-2.0***
- ***openssl***
- ***xmlsec1***
- ***xmlsec1-openssl***
- ***json-c***
- ***dbus-1***

This package also requires either ***libzip*** (version >= 0.11) 
or the binaries ***zip*** and ***unzip***. By default, it will
use ***libzip***.

### Compiling

The main scheme for compiling the project is:

> cmake .
> make
> sudo make install

By default, the installation is made in ***/usr***.
To change this behaviour, you should set the variable
CMAKE_INSTALL_PREFIX as in the below example:

> cmake -DCMAKE_INSTALL_PREFIX=$(pwd)/root .

You could check the documentation of the standard CMake module 
[GNUInstallDirs](https://cmake.org/cmake/help/v3.4/module/GNUInstallDirs.html).

To forbid the use of ***libzip*** and replace it with the
use of programs ***zip*** and ***unzip***, type:

> cmake -DUSE_LIBZIP=0 .

## Content

This package content source files for several programs.
The installed programs are:

- ***afm-system-daemon***: D-Bus daemon to install,
  uninstall, list the widgets.

  It runs on the system bus.

- ***afm-user-daemon***: D-Bus daemon to list
  available widgets, to start, pause, resume, stop it.

  It runs on the user session bus.

- ***wgtpkg-info***: command line tool to display
  informations about a widget file.

- ***wgtpkg-installer***: command line tool to
  install a widget file.

- ***wgtpkg-pack***: command line tool to create
  a widget file from a widget directory.

- ***wgtpkg-sign***: command line tool to add a signature
  to a widget directory.

## Comparison with Tizen framework

This package is providing few less behaviour than
the following Tizen packages:

- platform/appfw/app-installers
- platform/core/security/cert-svc
- platform/core/appfw/ail
- platform/core/appfw/aul-1
- platform/core/appfw/libslp-db-util

## Links

### Details about widgets


### Details about dependencies

For details, you can dig into internet the following links:

- [libxml2](http://xmlsoft.org/html/index.html)
- [OpenSSL](https://www.openssl.org)
- [XMLSec](https://www.aleksey.com/xmlsec)
- [JSON-c](https://github.com/json-c/json-c)
- [D-Bus](http://www.freedesktop.org/wiki/Software/dbus)
- [libzip](http://www.nih.at/libzip)
- [CMake](https://cmake.org)


