# Configuration of redpesk packages

Applications created for redpesk OS can include a subdirectory named `.rpconfig`.

That directory holds files used by redpesk OS framework.

## Overview

When the directory `.rpconfig` is present in installed files its content is scanned
for finding files of names:

- `manifest.yml`: description of the application
- `signature-author.p7`: signature of the author
- `signature-XXX.p7`: signature of distributors

If these files are found, it is used by redpesk OS application framework to setup
the installed RPM.

## The manifest file

The file `.rpconfig/manifest.yml` is a YAML file describing for the framework
important data of the installed RPM.

Here is an example of manifest file:

```yaml
rp-manifest: 1

id: SmartHome
version: 1
author: Qt team
license: GPL

description: >
  This is the Smarthome QML demo application. It shows some user interfaces for controlling an
  automated house. The user interface is completely done with QML.

targets:
  - target: main
    content:
	src: /usr/share/smarthome/smarthome.qml
	type: text/vnd.qt.qml
    icon:
	src: /usr/share/smarthome/smarthome-icon64x64.jpg
	type: image/jpeg
	size: { x: 64, y: 64 }
```

The manifest is made of two parts:

- the global part: defines features globally
- the targets part: defines the targets one by one

The global part MUST have the following fields:

- rp-manifest
- id
- version

The global part MAY have the following fields:

- description
- author
- license
- file-properties
- required-permission
- provided-binding

The target part MUST have the following fields:

- target
- content

In all cases, unknown attributes are silently ignored.

## Global part of the manifest file

### rp-manifest

The attribute *rp-manifest* is mandatory. It must be set to the value `1.0`.

### id

The attribute *id* is mandatory.

Values for *id* are any non empty string containing only latin letters,
arabic digits, and the three characters '.' (dot), '-' (dash) and
'\_' (underscore).

### version

The attribute *version* is mandatory.

Values for *version* are any non empty string containing only latin letters,
arabic digits, and the three characters '.' (dot), '-' (dash) and
'\_' (underscore).

Version values are dot separated fields MAJOR.MINOR.REVISION.
Such version would preferably follow guidelines of
[semantic versioning][semantic-version].

### name

The attribute *name* is optional. If not set, the *id* is taken as a replacement.

### file-properties

Use this feature for setting properties to files or directories of the package.

Example:

```yaml
file-properties:
  - name: /usr/bin/flite
    value: executable
  - name: /usr/bin/jtalk
    value: executable
```

The name is the relative path of the file whose property
must be set.

The value must be one of:

- executable: the file is executable
- public: the file is public read only
- library: the file is a private library
- config: the file is a private configuration
- data: the file is a private data (default)
- www: the file is world wide read only


### provided-binding

This feature allows to export a binding to other binders.
The bindings whose relative name is given as value is exported to
other binders under the given name.

Example:

```yaml
provided-binding:
   - name: extra
     value: export/binding-gps.so
```

Exports a local binding to other applications.

The value must contain the path of the exported binding.


### required-permission

Dictionnary of the permissions required globally, meaning by each target.

Example:

```yaml
required-permission:
  urn:AGL:permission:real-time:
    name: urn:AGL:permission:real-time
    value: required
  urn:AGL:permission:syscall:
    name: urn:AGL:permission:syscall:*
    value: required
```

The key is the value of the required permission.

The name is optional.

The value is either:

- required: the permission is required
- optional: the permission is optional

### plugs

Use this feature for exporting files to applications
able to receive plugins.

Example:

```yaml
plugs:
  - name: canbus/plug
    value: canbus-binding
```

The name is the name of the exported directory.

The value is the identifier of the application
to which the files are exported.



## Targets part of the manifest file

Targets are startable units of the package.

More than one target can be given.

If a target is given, at least one must be of id **main**.

### target

Identifier of the target. Mandatory.

At least one target must be named **main**.

### content

The attribute *content* is mandatory and must designate a file
(subject to localization) with its mandatories attributes *src* and *type*.

The content designed depends on its type. See [the known types][known-content].

### icon

The attribute *icon* is optional.
It must designate an image file with its attribute *src*.

The type and the size can optionally be given.

### name

The attribute *name* is optional.

It is used for naming the target. If not defined, the value of attribute target is used.

### description

The attribute *description* is optional.

This attribute is used for describing the target.

### required-config

List of configuration files for the binder.

Example:

```yaml
required-config:
   - etc/config-main.json
   - etc/config-aux1.json
   - etc/config-aux2.json
```

### required-api

List of the api required by the target.

Example:

```yaml
required-api:
   - name: gps
     value: auto
   - name: afm-main
     value: link
```

The name is the name of the required API.

The value describes how to connect to the required api.
It is either:

- auto:
  The framework set automatically the kind of
  the connection to the API

- ws:
  The framework connect using internal websockets

- dbus: [OBSOLETE, shouldn't be used currently]
  The framework connect using internal dbus

- tcp:
  In that case, the name is the URI to access the service.
  The framework connect using a URI of type
   HOST:PORT/API
  API gives the name of the imported api.

- cloud: [PROPOSAL - NOT IMPLEMENTED]
  The framework connect externally using websock.
  In that case, the name includes data to access the service.
  Example: `<param name="log:https://oic@agl.iot.bzh/cloud/log" value="cloud" />`

### required-binding

List of the bindings required by the package.

Example:

```yaml
required-binding:
   - name: libexec/binding-gps.so
     value: local
   - name: extra
     value: extern
```

The name or the path of the required BINDING.

The value describes how to connect to the required binding.
It is either:

- local:
  The binding is a local shared object.
  In that case, the name is the relative path of the
  shared object to be loaded.

- extern:
  The binding is external. The name is the exported binding name.
  See provided-binding.

### provided-api

Use this feature for exporting one or more API of a unit
to other packages of the platform.

This feature is an important feature of the framework.

Example:

```yaml
provided-api:
  - name: geoloc
    value: auto
  - name: moonloc
    value: auto
```

The name give the name of the api that is exported.

The value is one of the following values:

- ws:
  export the api using UNIX websocket

- dbus: [EXPERIMENTAL]
  export the API using dbus

- auto:
  export the api using the default method(s).

- tcp:
  In that case, the name is the URI used for exposing the service.
  The URI is of type
   HOST:PORT/API
  API gives the name of the exported api.

### required-permission

This attribute is the same than the global one but allows to require
specific permissions for specific target.

### required-systemd

These settings establishe a requirement of the application to
systemd's units (socket, target, service, mount, ...).

It enforce the application to start after the listed dependencies.

The setting lists the required units. For each 2 fields
are required:

- `unit`: the name of the required unit with its extension
- `mode`: the mode of the dependency

Valid values for modes are:

- `weak`: translated to systemd's directive *Wants*
- `strong`: translated to systemd's directive *Requires*
- `strict`: translated to systemd's directive *BindsTo*


Example:

```yaml
required-systemd:
   - unit: base.target
     mode: strict
   - unit: foo.socket
     mode: strong
   - unit: bar.service
     mode: weak
```




[known-content]:    ./known-content-types.html                                        "Known content types"

