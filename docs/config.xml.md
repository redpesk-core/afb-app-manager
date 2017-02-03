The configuration file config.xml
===========

The widgets are described by the W3C's technical recommendations
[Packaged Web Apps (Widgets)][widgets] and [XML Digital Signatures for Widgets][widgets-digsig]
 that specifies the configuration file **config.xml**.

Overview
--------

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

- **<widget id="......"\>**: gives the id of the widget. It must be unique.

- **<widget version="......"\>**: gives the version of the widget

- **<icon src="..."\>**: gives a path to the icon of the application
  (can be repeated with different sizes)

- **<content src="..." type="..."\>**: this indicates the entry point and its type.

Standard elements of "config.xml"
---------------------------------

### The element widget

#### the attribute id of widget

The attribute *id* is mandatory (for version 2.x, blowfish) and must be unique.

Values for *id* are any non empty string containing only latin letters,
arabic digits, and the three characters '.' (dot), '-' (dash) and
'_' (underscore).

Authors can use a mnemonic id or can pick a unique id using
command **uuid** or **uuidgen**.

### the attribute version of widget

The attribute *version* is mandatory (for version 2.x, blowfish).

Values for *version* are any non empty string containing only latin letters,
arabic digits, and the three characters '.' (dot), '-' (dash) and
'_' (underscore).

Version values are dot separated fields MAJOR.MINOR.REVISION.
Such version would preferabily follow guidelines of
[semantic versionning][semantic-version].

### The element content

The element *content* is mandatory (for version 2.x, blowfish) and must designate a file
(subject to localisation) with its attribute *src*.

The content designed depends on its type. See below for the known types.

### The element icon

The element *icon* is mandatory (for version 2.x, blowfish) and must
be unique. It must designate an image file with its attribute *src*.

AGL features
------------

The AGL framework uses the feature tag for specifying security and binding
requirement of the widget.

The current version of AGL (up to 2.0.1, blowfish) has no fully implemented
features.

The features planned to be implemented are described below.

### feature name="urn:AGL:widget:required-api"

List of the api required by the widget.

Each required api must be explicited using a <param> entry.

Example:
```xml
<feature name="urn:AGL:widget:required-api">
  <param name="urn:AGL:permission:A" value="required" />
  <param name="urn:AGL:permission:B" value="optional" />
</feature>
```

This will be *virtually* translated for mustaches to the JSON
```json
"required-api": {
  "param": [
      { "name": "urn:AGL:permission:A", "value": "required", "required": true },
      { "name": "urn:AGL:permission:A", "value": "optional", "optional": true }
    ],
  "urn:AGL:permission:A": { "name": "urn:AGL:permission:A", "value": "required", "required": true },
  "urn:AGL:permission:B": { "name": "urn:AGL:permission:B", "value": "optional", "optional": true }
}
```

#### param name="#target"

Declares the name of the component requiring the listed bindings.
Only one instance of the param "#target" is allowed.
When there is not instance of the param
The value is either:

- required: the binding is mandatorily needed except if the feature
isn't required (required="false") and in that case it is optional.
- optional: the binding is optional

#### param name=[required api name]

The value is either:

- required: the binding is mandatorily needed except if the feature
isn't required (required="false") and in that case it is optional.
- optional: the binding is optional

### feature name="urn:AGL:widget:required-permission"

List of the permissions required by the widget.

Each required permission must be explicited using a <param> entry.

#### param name=[required permission name]

The value is either:

- required: the permission is mandatorily needed except if the feature
isn't required (required="false") and in that case it is optional.
- optional: the permission is optional

### feature name="urn:AGL:widget:provided-api"

Use this feature for each provided api of the widget.
The parameters are:

#### param name="subid"

REQUIRED

The value is the string that must match the binding prefix.
It must be unique.

#### param name="name"

REQUIRED

The value is the string that must match the binding prefix.
It must be unique.

#### param name="src"

REQUIRED

The value is the path of the shared library for the binding.

#### param name="type"

REQUIRED

Currently it must be ***application/vnd.agl.binding.v1***.


#### param name="scope"

REQUIRED

The value indicate the availability of the binidng:

- private: used only by the widget
- public: available to allowed clients as a remote service (requires permission+)
- inline: available to allowed clients inside their binding (unsafe, requires permission+++)

#### param name="needed-binding"

OPTIONAL

The value is a space separated list of binding's names that the binding needs.

### feature name="urn:AGL:widget:defined-permission"

Each required permission must be explicited using a <param> entry.

#### param name=[defined permission name]

The value is the level of the defined permission.
Standard levels are: 

- system
- platform
- partner
- tiers
- public

This level defines the level of accreditation required to get the given
permission. The accreditions are given by signatures of widgets.

Known content types
-------------------

The configuration file ***/etc/afm/afm-unit.conf*** defines the types
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
[semantic-version]: http://semver.org/                                              "Semantic versionning"



