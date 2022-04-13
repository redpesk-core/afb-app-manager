# afb-app-manager

## Overview

This repository is named **afb-app-manager** because
it stands for **AFB Application Manager**.

It contains programs and services to create widgets,
to install widgets, to run widgets.

## How to compile?

This project uses CMAKE and C compiler suite to be compiled.

### Dependencies

This package requires the following libraries or modules:

- ***libxml-2.0***
- ***xmlsec1***
- ***xmlsec1-openssl***
- ***openssl***

- ***json-c***

- ***dbus-1***

- ***sec-lsm-manager***

This package also requires either ***libzip*** (version >= 0.11)
or the binaries ***zip*** and ***unzip***. By default, it will
use ***libzip***.

### Compiling

The main scheme for compiling the project is:

> cmake .
>
> make
>
> sudo make install

By default, the installation is made in ***/usr/local***.
To change this behaviour, you should set the variable
CMAKE_INSTALL_PREFIX as in the below example:

> cmake -DCMAKE_INSTALL_PREFIX=/some/where .

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

- ***wgtpkg-info***: command line tool to display
  information about a widget file.

- ***wgtpkg-install***: command line tool to
  install a widget file.

- ***wgtpkg-pack***: command line tool to create
  a widget file from a widget directory.

- ***wgtpkg-sign***: command line tool to add a signature
  to a widget directory.

## Description

### Actors

The framework defined by afb-app-manager is defining several actors:
the platform designer, the application developer, the distributor,
the user, the hacker.

The platform designer defines the AGL system and its security.

The application developer in link or not with hardware vendors
is creating applications, modules, libraries, services that will
be installed to the platform.

The hacker is a user that also develops application for
tuning its system.

The distributor is the mediator between the developer and the
user. It provides

The user is either the driver or a passenger of the car.

The application, libraries, services are available on the
platform. Some of them are in direct interaction with users.
Some others, like services, are used indirectly.

### Scenarii

#### Writing applications

The application will receive an identifier.
That identifier must have the following feature:

- it must be unique to identify the application and its revisions
- it should be short enough to be used with efficiency by
  security components of the system
- it can not be stolen by malicious applications that
  would like to spoof the application identity
- it can be sold to other company

The framework provide a facility to create an asymetric
key that will serve all the above purposes (it currently
doesn't).

Using its favorite environment, the developer
produces applications for the target.

Depending on its constraints either economic,
technical or human, the developer chooses the language
and the environment for developing the applications.

This step needs to test and to debug the application on
a target or on a simulator of the target.
In both cases, the code should be lively inspected and
changed, as well as the permissions and the security
aspects.

The framework will provide facilities for debugging
(it currently doesn't).

#### Packaging applications

Currently the framework expects widgets packaged as
specified by [Packaged Web Apps](http://www.w3.org/TR/widgets).

When the application is ready, the developer
creates a package for it. The creation of the package
is made of few steps:

- isolate the strict necessarily files and structure it
  to be children of only one root directory
- sign the application with the developer key
- sign the application with its application key
- pack the application using zip

The framework will provide facilities to package applications.

Parts of the job can be done with tools provided by afb-app-manager:

- ***wgtpkg-sign*** is used to add signatures at root of the package
- ***wgtpkg-pack*** is used to create the package file (with wgt extension).

Currently, the ***config.xml*** file must be edited by hand.
See below [Writing the config.xml](#writing-config).

#### Distributing applications

Normally a store will distribute the application.
It will be the normal process. The distributor adds
a signature to the distributed application.

The added signature can allow more or less permission to
applications. For example, a critical application nested
in the system should have high level permissions allowing
it to do things that should normally not be done (changing
system configuration for example).
To allow such application, the distributor must sign
it using its secret private key that will unlock the
requested level of permissions.

Currently, the framework allows to make these steps manually
using ***unzip***, ***wgtpkg-sign*** and ***wgtpkg-pack*** utilities.

Applications of the store will then be available
for browsing and searching over HTTP/Internet.

#### Installing applications

The framework will provide an API for downloading and
installing an application from stores (it currently doesn't).

The current version of afm allows to install widgets
from local files (either pre-installed or downloaded).

To install a widget, you can use either the program
***wgtpkg-install*** while being the framework user.

## Cryptography

The widgets are currently signed and checked using the library
[XMLSec](https://www.aleksey.com/xmlsec).

The current state isn't providing our keys.
Will be done soon.

TO BE CONTINUED

## Extension to the packaging specifications

The widgets are specified in that W3C recommendation:
[Packaged Web Apps](http://www.w3.org/TR/widgets).
This model was initially designed for HTML applications.
But it is well suited for other kind of applications.

It relies on this specification that is the master
piece of interest and the most useful part:
[XML Digital Signatures for Widgets](http://www.w3.org/TR/widgets-digsig).

An other specification exist that isn't either mature
nor suited for managing privileges:
[Web App Manifest](http://www.w3.org/TR/appmanifest).
However, it may become of actuallity in some future.

The main idea is to use the file ***config.xml*** as a switch
for several constants.
The current specifications for ***config.xml*** are allowing
to describe either HTML5, QML and native applications.
Using *feature*, it is also possible to define uses of
libraries.

For more advanced uses like:

- incremental updates
- multiple application packages
- system updates

The file ***config.xml*** may:

- either, contain a root different that *widget*
- or, not exist, being replaced with something else.



## Links

- [Packaged Web Apps](http://www.w3.org/TR/widgets)
- [XML Digital Signatures for Widgets](http://www.w3.org/TR/widgets-digsig)
- [libxml2](http://xmlsoft.org/html/index.html)
- [OpenSSL](https://www.openssl.org)
- [XMLSec](https://www.aleksey.com/xmlsec)
- [JSON-c](https://github.com/json-c/json-c)
- [D-Bus](http://www.freedesktop.org/wiki/Software/dbus)
- [libzip](http://www.nih.at/libzip)
- [CMake](https://cmake.org)
- [Sec-lsm-Manager](https://wiki.tizen.org/wiki/Security/Tizen_3.X_Security_Manager)
- [Web App Manifest](http://www.w3.org/TR/appmanifest)
