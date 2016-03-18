
The widgets
===========

    version: 1
    Date:    15 March 2016
    Author:  José Bollo

The widgets
-----------

The widgets are described by the technical recommendations
[widgets] and [widgets-digsig].

### signature of the 

The application framework 

This is the original part of our work here

### directory where are stored applications

Applications can be installed in few places: on the system itself or on an extension device.
For my phone, for example, it is the sd card.

This translates to:

 - /usr/applications: for system wide applications
 - /opt/applications: for removable applications

In the remaining of the document, these places are writen "APPDIR".


Organisation of directory of applications
=========================================

The main path for applivcations are: APPDIR/PKGID/VER.

Where:

 - APPDIR is as defined above
 - PKGID is a directory whose name is the package identifier
 - VER is the version of the package MAJOR.MINOR

This organisation has the advantage to allow several versions to leave together.
This is needed for some good reasons (rolling back) and also for less good reasons (user habits).

Identity of installed files
---------------------------

All the files are installed as the user "userapp" and group "userapp".
All files have rw(x) for user and r-(x) for group and others.

This allows any user to read the files.


Labelling the directories of applications
-----------------------------------------


Organisation of data
====================

The data of a user are in its directory and are labelled using the labels of the application




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




