# afb-app-manager

## Overview

This repository is named **afb-app-manager** because
it stands for **AFB Application Manager**.

It contains programs and services to create redpesk
microservices, to install it and to run it.

## How to compile?

This project uses CMAKE and C compiler suite to be compiled.

### Dependencies

This package requires the following libraries or modules:

- ***json-c***
- ***sec-lsm-manager***
- ***gnutsl***
- ***librp-utils***
- ***libsystemd***
- ***dbus-1***
- ***afb-binding***
- ***rpm***

If legacy widgets are to be supported

- ***libxml-2.0***
- ***xmlsec1***
- ***xmlsec1-gnutls***
- ***xmlsec1-gcrypt***
- either ***libzip*** (version >= 0.11) or the binaries ***zip*** and ***unzip***

### Compiling

The main scheme for compiling the project is:

```bash
> cmake .
>
> make
>
> sudo make install
```

By default, the installation is made in ***/usr/local***.
To change this behaviour, you should set the variable
CMAKE_INSTALL_PREFIX as in the below example:

```bash
> cmake -DCMAKE_INSTALL_PREFIX=/some/where .
```

You could check the documentation of the standard CMake module
[GNUInstallDirs](https://cmake.org/cmake/help/v3.4/module/GNUInstallDirs.html).

To forbid the use of ***libzip*** and replace it with the
use of programs ***zip*** and ***unzip***, type:

```bash
> cmake -DUSE_LIBZIP=0 .
```
