# Using plugs within redpesk framework

Redpesk framework installs applications which it manages in
separate directories and isolates them from each other
using Linux security modules like Smack or SELinux.

The plug mechanism allows an application to export parts
of it to specific applications, breaking the isolation rules
on purpose.

This document gives use cases, explains how it works,
and how to use it.

## Plugins use case

The main use case, but not the only one, is plugins.

A plugin is a piece of data and/or code used to add features
to some other component.
In other words, a plugin is an extension mechanism
that allows the addition of features to an application.

In the history of redpesk framework, the need emerged
for the CAN (Controller Area Network) implementation. Redpesk provides an
application dedicated to handling the CAN bus for its clients.
It wraps control of the bus within a single point and
provides a high level API.

To offer the high level API to its clients, an API where
binary encoding of data is hidden and where data are
named, it receives extensions (plugins) that describe
the currently connected equipment and the encoding of
data sent on CAN.

For example, if you install equipment (hardware) on the CAN
bus, you also install its related plugin (software) that
describes it to the CAN bus application.

As it could be deduced from the description, the CAN bus
application is installed once and the plugins that describe
the equipment are installed independently afterwards.

Since canbus-binding 2.0.3, the application CAN bus
installed in the directory `/usr/redpesk/canbus-binding`
has a directory for plugging plugins which
is `/usr/redpesk/canbus-binding/plugins`. On startup, the
application canbus-binding 2 scans recursively the contents
of the directories:

- `${AFB_ROOTDIR}/etc`
- `${AFB_ROOTDIR}/plugins`
- `${CANBUS_PLUGINS_PATH}`

It loads every file with `.json` or `.so` as an extension,
presuming them to be configuration and extension.
Items deemed compatible with canbus-binding are then activated
and made available to clients of the canbus-binding application.

## How plugs work?

The plug mechanism creates a symbolic link to an exported
directory in a directory.

Example: the plugin vcar-signals is an application installed in
`/usr/redpesk/vcar-signals`. It contains the plugin library
`/usr/redpesk/vcar-signals/lib/plugin-vcar-signals.so`. To make
that plugin available to canbus-binding, the manifest file
of vcar-signals declares that it plugs its lib directory in
the plugin directory of canbus-binding. That declaration asks
the framework to create the symbolic link below during installation
of vcar-signals.

```
/usr/redpesk/canbus-binding/plugins/vcar-signals -> /usr/redpesk/vcar-signals/lib
```

Using this example, it is possible to introduce some terminology.

- **exported directory**: the exported directory is the directory
  that the installed application exports (in the example, the
  exported directory is `/usr/redpesk/vcar-signals/lib`)

- **import directory**: the directory where the exported directory
  is plugged, or in other words, the directory where the
  symbolic link is created (in the example, the import
  directory is `/usr/redpesk/canbus-binding/plugins`)

- **exporting application**: the application that exports a directory
  (in the example, vcar-signals)

- **import application**: the application that receives access to the
  exported directory (in the example, canbus-binding)

### The created link

The import directory is deduced from the ID of the import application
(`<importid>`), it is `/redpesk/<importid>/plugins`.

The exported directory is given by the path (`<expdir>`)
that must be relative to the root dir of the installed application
(`<rootdir>`), it is `<rootdir>/<expdir>`.

The name of the link is the application ID of the exporting application
(`<exportid>`).

So in the end, the following link is created:

```
/redpesk/<importid>/plugins/<exportid> -> <rootdir>/<expdir>
```

### Security

Installing an extension should be under control in order to avoid
a malicious actor introducing a vulnerability in the system.

When plugs are used for exporting to the import application of ID
`<importid>`, the exporting application must have the permission
`urn:redpesk:permission:<importid>:partner:export:plug` if the import
application does not have the permission
`urn:redpesk:permission::public:plugs`.

Otherwise, if the import application has the permission
`urn:redpesk:permission::public:plugs`, the exporting application
must have the permission
`urn:redpesk:permission:<importid>:public:export:plug`.

### Rules of use

1. An application can export any of its directories
2. An application can export only one directory to another application
3. An application can export the same directory to many applications
4. An application can export more than a single directory
5. The contents of the directory is recursively made available to import
   applications
6. An import application must have the directory `/redpesk/<importid>/plugins`

## How to use plugs?

Plug behaviour is only available in `manifest.yml` files.

The `manifest.yml` file accepts a root entry named `plugs`
that contains a list of objects having the entries `name`
and `value`. The name is the relative path of the exported
directory and the value is the application ID of the import
application.

Example:

```yaml
plugs:
   - name: lib
     value: canbus-binding
```

## History

- 2023-12-18, Louis-Baptiste Sobolewski & Frances Thompson, proofreading

- 2023-12-15, José Bollo, Creation for version afb-app-manager 12.2.2 and
  sec-lsm-manager 2.6.1
