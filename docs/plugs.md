# Using plug within redpesk framework

Redpesk framework installs applications it manages in
separate directories and isolates them from each others
using linux security behaviors like Smack or SELinux.

The plug mechanism allows an application to export parts
of it to specific applications, breaking the isolation rules
on purpose.

This document gives use cases, explains how it works,
details how to use it.

## Plugins use case

The main use case, but not the only one, is plugins.

A plugin is a piece of data and/or code that the use
of plugins to add features to some other component.
In other words, a plugin is an extension mechanism
that allows to add features to an application.

In the history of redpesk framework, the case occurred
for CAN (Controller Area Network). Redpesk provides an
application dedicated the handling CAN bus for its clients.
It wraps control of the bus within a single point and
brings an high level API to its clients.

To offer the high level API to its clients, an API where
binary encoding of data is hidden and where data are
named, it receives extensions (plugins) that describe
the currently connected equipment and the encoding of
data sent on CAN.

For example, if you install an equipment (hardware) on the CAN
bus, you also install its related plugin (software) that
describe it to the CAN bus application.

As it could be deduced from the description, the CAN bus
application is installed once and the plugins that describe
the equipment are installed independently afterwards.

Since canbus-binding 2.0.3, the application CAN bus
installed in the directory `/usr/redpesk/canbus-binding`
has a directory for plugging plugins, that directory
is `/usr/redpesk/canbus-binding/plugins`. At start, the
application canbus-binding 2 scans recursively the content
of the directories:

- ${AFB_ROOTDIR}/etc
- ${AFB_ROOTDIR}/plugins
- ${CANBUS_PLUGINS_PATH}

It loads every file with extension `.json` and every file with
extension `.so`, presuming it to be configuration and extension.
The found items compatible with canbus-binding are then activated
and made available to clients of canbus-binding application.

## How plug works?

The plug mechanism creates a symbolic link to an exported
directory in a directory.

Example: The plugin vcar-signals is an application installed in
`/usr/redpesk/vcar-signals`. It contains the plugin library
`/usr/redpesk/vcar-signals/lib/plugin-vcar-signals.so`. To make
that plugin available to canbus-binding, the manifest file
of vcar-signals declares that it plugs its lib directory in
the plugin directory of canbus-binding. That declaration ask
the framework to create the below symbolic link during installation
of vcar-signals.

```
/usr/redpesk/canbus-binding/plugins/vcar-signals -> /usr/redpesk/vcar-signals/lib
```

Using this example, it is possible to introduce little terminology.

- **exported directory**: the exported directory is the directory
  that the installed application exports (for the example, the
  exported directory is `/usr/redpesk/vcar-signals/lib`)

- **import directory**: the directory where is plugged the exported
  directory, or said with other words, the directory where the
  symbolic link is created (for the example, the import
  directory is `/usr/redpesk/canbus-binding/plugins`)

- **exporting application**: the application that export a directory
  (for the example, vcar-signals)

- **import application**: the application that receives access to the
  exported directory (for the example, canbus-binding)

### The created link

The import directory is deduced from the id of the import application
(`<importid>`), it is `/redpesk/<importid>/plugins`.

The exported directory is given by the path (`<expdir>`)
that must be relative to the root dir of the installed application
(`<rootdir>`), it is `<rootdir>/<expdir>`.

The name of the link is the application id of the exporting application
(`<exportid>`).

So at the end, the following link is created:

```
/redpesk/<importid>/plugins/<exportid> -> <rootdir>/<expdir>
```

### Security

Installing an extension should be under control in order to avoid
a malicious actor to install an extension that breaks the system.

When plug is used for exporting to the import application of id
`<importid>`, the exporting application must have the permission

```
urn:redpesk:permission:<importid>:partner:export:plug
```

If the import application has not the permission
`urn:redpesk:permission::public:plugs`.

Otherwise, if the import application has the permission
`urn:redpesk:permission::public:plugs`, the exporting application
must have the permission

```
urn:redpesk:permission:<importid>:public:export:plug
```

### Rules of use

1. An application can export any of its directory
2. An application can export only one directory to an other application
3. An application can export a same directory to many applications
4. An application can export more than a single directory
5. the content of the directory is made recursively available to import
   applications
6. An import application must have the directory `/redpesk/<importid>/plugins`

## How to use plug?

Plug behaviour is only available in `manifest.yml` files.

The  `manifest.yml` file accept a root entry named `plugs`
that contain a list of objects having the entries `name`
and `value`. The name is the relative path of the exported
directory and the value is the application id of the import
application.

Example:

```
plugs:
   - name: lib
     value: canbus-binding
```

## History

- 2023-12-15, José Bollo, Creation for version afb-app-manager 12.2.2 and
  sec-lsm-manager 2.6.1
