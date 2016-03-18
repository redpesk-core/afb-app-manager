
The afm-system-daemon
=====================

    version: 1
    Date:    15 March 2016
    Author:  José Bollo



Foreword
--------

This document describes what we intend to do. It may happen that our
current implementation and the content of this document differ.

In case of differences, it is assumed that this document is right
and the implementation is wrong.


Introduction
------------

The daemon **afm-system-daemon** is in charge of installing
applications on the system. Its main tasks are:

 - installs the applications and setup the security framework
   to include it

 - uninstall the applications

The **afm-system-daemon** takes its orders from the system
instance of D-Bus.

The figure below summarizes the situation of the
**afm-system-daemon** in the system.

    +------------------------------------------------------------+
    |                          User                              |
    |                                                            |
    |     +-------------------------------------------------+    |
    |     |                                                 |    |
    |     |                  afm-user-daemon                |    |
    |     |                                                 |    |
    |     +----------+----------------------+----------+----+    |
    |                |                      |          :         |
    |                |                      |          :         |
    :================|======================|==========:=========:
    |                |                      |          :         |
    |     +----------+----------+     +-----+-----+    :         |
    |     |   D-Bus   system    +-----+  CYNARA   |    :         |
    |     +----------+----------+     +-----+-----+    :         |
    |                |                      |          :         |
    |     +----------+---------+    +-------+----------+----+    |
    |     | afm-system-daemon  +----+   SECURITY-MANAGER    |    |
    |     +--------------------+    +-----------------------+    |
    |                                                            |
    |                          System                            |
    +------------------------------------------------------------+


Starting **afm-system-daemon**
------------------------------

**afm-system-daemon** is launched as a **systemd** service
attached to system. Normally, the service file is
located at /lib/systemd/system/afm-system-daemon.service.

The options for launching **afm-system-daemon** are:

    -r
    --root directory
    
         Set the root application directory.

         Note that the default root directory is defined
         to be /usr/share/afm/applications (may change).
    
    -d
    --daemon
    
         Daemonizes the process. It is not needed by sytemd.
    
    -q
    --quiet
    
         Reduces the verbosity (can be repeated).
    
    -v
    --verbose
    
         Increases the verbosity (can be repeated).
    
    -h
    --help
    
         Prints a short help.
    
The D-Bus interface
-------------------

### Overview of the dbus interface

***afm-system-daemon*** takes its orders from the session instance
of D-Bus. The use of D-Bus is great because it allows to implement
discovery and signaling.

The **afm-system-daemon** is listening with the destination name
***org.AGL.afm.system*** at the object of path ***/org/AGL/afm/system***
on the interface ***org.AGL.afm.system*** for the below detailed
members ***install*** and ***uninstall***.

D-Bus is mainly used for signaling and discovery. Its optimized
typed protocol is not used except for transmitting only one string
in both directions.

The client and the service are using JSON serialisation to
exchange data. 

The D-Bus interface is defined by:

 * DESTINATION: **org.AGL.afm.system**

 * PATH: **/org/AGL/afm/system**

 * INTERFACE: **org.AGL.afm.system**

The signature of any member of the interface is ***string -> string***
for ***JSON -> JSON***.

This is the normal case. In case of error, the current implmentation
returns a dbus error that is a string.

Here is an example that use *dbus-send* to query data on
installed applications.

    dbus-send --session --print-reply \
        --dest=org.AGL.afm.system \
        /org/AGL/afm/system \
        org.AGL.afm.system.install 'string:"/tmp/appli.wgt"'

### The protocol over D-Bus

---

#### Method org.AGL.afm.system.install

**Description**: Install an application from its widget file.

If an application of the same *id* and *version* exists, it is not
reinstalled except if *force=true*.

Applications are installed in the subdirectories of the common directory
of applications.
If *root* is specified, the application is installed under the
sub-directories of the *root* defined.

Note that this methods is a simple accessor to the method
***org.AGL.afm.system.install*** of ***afm-system-daemon***.

After the installation and before returning to the sender,
***afm-system-daemon*** sends the signal ***org.AGL.afm.system.changed***.

**Input**: The *path* of the widget file to install and, optionaly,
a flag to *force* reinstallation, and, optionaly, a *root* directory.

Either just a string being the absolute path of the widget file:

    "/a/path/driving/to/the/widget"

Or an object:

    {
      "wgt": "/a/path/to/the/widget",
      "force": false,
      "root": "/a/path/to/the/root"
    }

"wgt" and "root" must be absolute paths.

**output**: An object with the field "added" being the string for
the id of the added application.

    {"added":"appli@x.y"}

---

#### Method org.AGL.afm.system.uninstall

**Description**: Uninstall an application from its id.


Note that this methods is a simple accessor to the method
***org.AGL.afm.system.uninstall*** of ***afm-system-daemon***.

After the uninstallation and before returning to the sender,
***afm-system-daemon*** sends the signal ***org.AGL.afm.system.changed***.

**Input**: the *id* of the application and, otpionaly, the path to
*root* of the application.

Either a string:

    "appli@x.y"

Or an object:

    {
      "id": "appli@x.y",
      "root": "/a/path/to/the/root"
    }

**output**: the value 'true'.

































[meta-intel]:       https://github.com/01org/meta-intel-iot-security                "A collection of layers providing security technologies"
[widgets]:          http://www.w3.org/TR/widgets                                    "Packaged Web Apps"
[widgets-digsig]:   http://www.w3.org/TR/widgets-digsig                             "XML Digital Signatures for Widgets"
[libxml2]:          http://xmlsoft.org/html/index.html                              "libxml2"
[openssl]:          https://www.openssl.org                                         "OpenSSL"
[xmlsec]:           https://www.aleksey.com/xmlsec                                  "XMLSec"
[json-c]:           https://github.com/json-c/json-c                                "JSON-c"
[d-bus]:            http://www.freedesktop.org/wiki/Software/dbus                   "D-Bus"
[libzip]:           http://www.nih.at/libzip                                        "libzip"
[cmake]:            https://cmake.org                                               "CMake"
[security-manager]: https://wiki.tizen.org/wiki/Security/Tizen_3.X_Security_Manager "Security-Manager"
[app-manifest]:     http://www.w3.org/TR/appmanifest                                "Web App Manifest"
[tizen-security]:   https://wiki.tizen.org/wiki/Security                            "Tizen security home page"
[tizen-secu-3]:     https://wiki.tizen.org/wiki/Security/Tizen_3.X_Overview         "Tizen 3 security overview"



