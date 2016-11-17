The widgets
===========

The widgets are described by the technical recommendations
[widgets] and [widgets-digsig].

In summary, **widgets are ZIP files that can be signed and
whose content is described by the file <config.xml>**.

The configuration file config.xml
---------------------------------

The file **config.xml** describes important data of the application
to the framework:

- the unique identifier of the application
- the name of the application
- the type of the application
- the icon of the application
- the permissions linked to the application
- the services and dependancies of the application

The file MUST be at the root of the widget and MUST be case sensitively name
***config.xml***.

The file **config.xml** is a XML file described by the document
[widgets].

Here is the example of the config file for the QML application SmartHome.

```xml
<?xml version="1.0" encoding="UTF-8"?>
<widget xmlns="http://www.w3.org/ns/widgets" id="smarthome" version="0.1">
  <name>SmartHome</name>
  <icon src="smarthome.png"/>
  <content src="qml/smarthome/smarthome.qml" type="text/vnd.qt.qml"/>
  <description>This is the Smarthome QML demo application. It shows some user interfaces for controlling an 
automated house. The user interface is completely done with QML.</description>
  <author>Qt team</author>
  <license>GPL</license>
</widget>
```

The most important items are:

- **\<widget id="......"\>**: gives the id of the widget. It must be unique.

- **\<widget version="......"\>**: gives the version of the widget

- **\<icon src="..."\>**: gives a path to the icon of the application
  (can be repeated with different sizes)

- **\<content src="..." type="..."\>**: this indicates the entry point and its type.
  The types handled are set through the file /etc/afm/afm-launch.conf

Further development will add handling of <feature> for requiring and providing
permissions and services.

---

### Standard elements of "config.xml"

#### The element widget

##### the attribute id of widget

The attribute *id* is mandatory (for version 2.x, blowfish) and must be unique.

Values for *id* are any non empty string containing only latin letters,
arabic digits, and the three characters '.' (dot), '-' (dash) and
'_' (underscore).

Authors can use a mnemonic id or can pick a unique id using
command **uuid** or **uuidgen**.

#### the attribute version of widget

The attribute *version* is mandatory (for version 2.x, blowfish).

Values for *id* are any non empty string containing only latin letters,
arabic digits, and the three characters '.' (dot), '-' (dash) and
'_' (underscore).

Version values are dot separated fields MAJOR.MINOR.REVISION.

#### The element content

The element *content* is mandatory (for version 2.x, blowfish) and must designate a file
(subject to localisation) with its attribute *src*.

The content designed depends on its type. See below for the known types.

#### The element icon

The element *icon* is mandatory (for version 2.x, blowfish) and must
be unique. It must designate an image file with its attribute *src*.

### Known widget types and content

The configuration file ***/etc/afm/afm-launch.conf*** defines the types
of widget known and how to launch it.

Known types for the type of content are (for version 2.x, blowfish):

- ***text/html***: 
   HTML application,
   content.src designates the home page of the application

- ***application/x-executable***:
   Native application,
   content.src designates the relative path of the binary

- ***application/vnd.agl.url***:
   Internet url,
   content.src designates the url to be used

- ***application/vnd.agl.service***:
   AGL service defined as a binder,
   content.src designates the directory of provided binders,
   http content, if any, must be put in the subdirectory ***htdocs*** of the widget

- ***application/vnd.agl.native***:
   Native application with AGL service defined as a binder,
   content.src designates the relative path of the binary,
   bindings, if any must be put in the subdirectory ***lib*** of the widget,
   http content, if any, must be put in the subdirectory ***htdocs*** of the widget

- ***text/vnd.qt.qml***, ***application/vnd.agl.qml***:
   QML application,
   content.src designate the relative path of the QML root,
   imports must be put in the subdirectory ***imports*** of the widget

- ***application/vnd.agl.qml.hybrid***:
   QML application with bindings,
   content.src designate the relative path of the QML root,
   bindings, if any must be put in the subdirectory ***lib*** of the widget,
   imports must be put in the subdirectory ***imports*** of the widget

- ***application/vnd.agl.html.hybrid***:
   HTML application,
   content.src designates the home page of the application,
   bindings, if any must be put in the subdirectory ***lib*** of the widget,
   http content must be put in the subdirectory ***htdocs*** of the widget

---

### AGL features

The AGL framework uses the feature tag for specifying security and binding
requirement of the widget.

The current version of AGL (up to 2.0.1, blowfish) has no fully implemented
features.

The features planned to be implemented are described below.

#### feature name="urn:AGL:required-binding"

List of the bindings required by the widget.

Each required binding must be explicited using a <param> entry.

##### param name=[required binding name]

The value is either:

- required: the binding is mandatorily needed except if the feature
isn't required (required="false") and in that case it is optional.
- optional: the binding is optional

#### feature name="urn:AGL:required-permission"

List of the permissions required by the widget.

Each required permission must be explicited using a <param> entry.

##### param name=[required permission name]

The value is either:

- required: the permission is mandatorily needed except if the feature
isn't required (required="false") and in that case it is optional.
- optional: the permission is optional

#### feature name="urn:AGL:provided-binding"

Use this feature for each provided binding of the widget.
The parameters are:

##### param name="name"

REQUIRED

The value is the string that must match the binding prefix.
It must be unique.

##### param name="src"

REQUIRED

The value is the path of the shared library for the binding.

##### param name="type"

REQUIRED

Currently it must be ***application/vnd.agl.binding.v1***.


##### param name="scope"

REQUIRED

The value indicate the availability of the binidng:

- private: used only by the widget
- public: available to allowed clients as a remote service (requires permission+)
- inline: available to allowed clients inside their binding (unsafe, requires permission+++)

##### param name="needed-binding"

OPTIONAL

The value is a space separated list of binding's names that the binding needs.

#### feature name="urn:AGL:defined-permission"

Each required permission must be explicited using a <param> entry.

##### param name=[defined permission name]

The value is the level of the defined permission.
Standard levels are: 

- system
- platform
- partner
- public

This level defines the level of accreditation required to get the given
permission. The accreditions are given by signatures of widgets.


Tools for managing widgets
--------------------------

This project includes tools for managing widgets.
These tools are:

- ***wgtpkg-info***: command line tool to display
  informations about a widget file.

- ***wgtpkg-installer***: command line tool to
  install a widget file.

- ***wgtpkg-pack***: command line tool to create
  a widget file from a widget directory.

- ***wgtpkg-sign***: command line tool to add a signature
  to a widget directory.

For all these commands, a tiny help is available with
options **-h** or **--help**.

There is no tool for unpacking a widget. For doing such operation,
you can use the command **unzip**.

To list the files of a widget:

```bash
$ unzip -l WIDGET
```

To extract a widget in some directory:

```bash
$ unzip WIDGET -d DIRECTORY
```

*Note that DIRECTORY will be created if needed*.

Getting data about a widget file
---------------------------------

The command **wgtpkg-info** opens a widget file, reads its **config.xml**
file and displays its content in a human readable way.

Signing and packing widget
--------------------------

### Signing

To sign a widget, you need a private key and its certificate.

The tool **wgtpkg-sign** creates or replace a signature file in
the directory of the widget BEFORE its packaging.

There are two types of signature files: author and distributor.

Example 1: add an author signature

```bash
$ wgtpkg-sign -a -k me.key.pem -c me.cert.pem DIRECTORY
```

Example 2: add a distributor signature

```bash
$ wgtpkg-sign -k authority.key.pem -c authority.cert.pem DIRECTORY
```

### Packing

This operation can be done using the command **zip** but
we provide the tool **wgtpkg-pack** that may add checking.

Example:
```bash
$ wgtpkg-pack DIRECTORY -o file.wgt
```
Writing a widget
----------------

### What kind of application?

The file **/etc/afm/afm-launch.conf** explain how to launch applications.
(It is the current state that use afm-user-daemon. In a future, it may be
replace by systemd features.)

Currently the applications that can be run are:

- binary applications: their type is ***application/x-executable***

- HTML5 applications: their type is ***text/html***

- QML applications: their type is ***text/vnd.qt.qml***

### The steps for writing a widget

1. make your application

2. create its configuration file **config.xml**

3. sign it

4. pack it

Fairly easy, no?

Organization of directory of applications
-----------------------------------------

### directory where are stored applications

Applications can be installed in different places: the system itself, extension device.
On a phone application are typically installed on the sd card.

This translates to:

 - /usr/applications: system wide applications
 - /opt/applications: removable applications

From here those paths are referenced as: "APPDIR".

The main path for applications is: APPDIR/PKGID/VER.

Where:

 - APPDIR is as defined above
 - PKGID is a directory whose name is the package identifier
 - VER is the version of the package MAJOR.MINOR

This organization has the advantage to allow several versions to leave together.
This is needed for some good reasons (rolling back) and also for less good reasons (user habits).

### Identity of installed files

All files are installed as user "afm" and group "afm".
All files have rw(x) for user and r-(x) for group and others.

This allows every user to read every file.

### labeling the directories of applications

The data of a user are in its directory and are labelled by the security-manager
using the labels of the application.

[widgets]:          http://www.w3.org/TR/widgets                                    "Packaged Web Apps"
[widgets-digsig]:   http://www.w3.org/TR/widgets-digsig                             "XML Digital Signatures for Widgets"
[libxml2]:          http://xmlsoft.org/html/index.html                              "libxml2"
[app-manifest]:     http://www.w3.org/TR/appmanifest                                "Web App Manifest"


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




