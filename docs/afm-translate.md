# Using afm-translate

This document the use of the tiny program `afm-translate`.

At time of writing this documentation, the future of this
command is not clear. It is a valuable program but might evolve
to become a more valuable tool.

## Goal of afm-translate

The program `afm-translate` takes a manifest file and produce
systemd unit files precursor. It accomplishes part of the job
made by afmpkg-installer.

The full featured afmpkg-installer processes manifests in the
way shown on the below diagram:

```
    manifest   metadata          template
         \      /                  /
          \    /                  /
           JSON                  /
             \                  /
              `--> (mustach) <-'
                      |
                      v
                   big-text
                      |
                      v
                   (splitter)
                      |
                      +--> unit1
                      +--> unit2
                      :
                      :
                      :
                      :
                      +--> unitX

```

The program `afm-translate` does the same except that:

- it does not split the output in units

- it takes metadata from its inputs where `afmpkg-installer`
  produces it internally

So the diagram showing  `afm-translate` process is:

```
    manifest   metadata          template
         \      /                  /
          \    /                  /
           JSON                  /
             \                  /
              `--> (mustach) <-'
                      |
                      v
                   big-text
```

The big text on the output contains the below directives,
each directive occupying one whole line starting with `%`:

- %nl
        produce an empty line at the end

- %begin systemd-unit
- %end systemd-unit
        delimit the produced unit

- %systemd-unit user
- %systemd-unit system
        tells the kind of unit (user/system)

- %systemd-unit service NAME
- %systemd-unit socket NAME
        gives the name and type of the unit

- %systemd-unit wanted-by NAME
        tells to install a link to unit in the wants of NAME


## Invocation of afm-translate

The command line invocation of `afm-translate` is one of the below.

### To get help

```
afm-translate -h
```

or

```
afm-translate --help
```

### To get version

```
afm-translate -v
```

or

```
afm-translate --version
```

### Normal operation


```
afm-translate [-l|-m|-s] [-u unitdir] [-o output] [-t template] manifest [meta...]"
```

where:

- `-l`, `-m` and `-s specify the translation method to use:
  legacy, modern or split.

  The default method is modern (option `-m`).

  The method `split` (option `-s`) produces a TAR file containing
  the generated files at their destination (see option `-u`).

  The method `legacy` (option `-l`) can be used to get output as given
  by versions before 0.2.

- `-u` specify the destination directory of generated units.
  It is used only by split method. If not set, the default
  destination is used (`/usr/local/lib/systemd`).

- `-o` can be used to set the output filename.
  If this option is not used, the result is sent to the standard output.

- `-t` specify the template mustache file to use.
  If not set, the default template file is used (`/etc/afm/afm-unit.conf`).

- `manifest` is the manifest file of an application, YAML or JSON

- `meta` is one or more file containing the meta data to merge
  to the manifest, YAML or JSON

By default, the template used is the template used
by `afmpkg-installer`

Caution, the metadata are using field name starting with a hash
sign that implies that the keys have to be in quotes if the file
is YAML encoded.

## Metadata

The used metadata are depending of the template. The default
template provided by afb-app-manager expects two kinds of
metadata:

- the global metadata
- the metadata per target

### Global metadata

The entry `#metadata` is at root. It contains the global metadata.

The global metadata are:

- root-dir: root directory of transaction (ex: /)

- install-dir: directory of installation, the directory that contains
  .rpconfig/manifest.yml (ex: /usr/redpesk/myapp)

- redpak: true or false, depending on being installed for redpak or not

- redpak-id: the redpak identifier (given by the environment variable
   AFMPKG\_ENVVAR\_REDPAKID on standard flow)

- icons-dir: optional and obsolete, points to the icon directory

### Target's metadata

The entry `#metatarget` is for targets. It is a dictionnary
containing target's metadata indexed by the target name.

The metadata for each targets are:

- afid: numeric id of the target

- http-port: port allocated to the target


### Example of metadata

So sample of metadata below

```yaml
"#metadata":
  root-dir: $HOME/app-data/useful
  install-dir: /usr/redpesk/useful
  icons-dir: /usr/redpesk/useful
  redpak: true
  redpak-id:

"#metatarget":
  main:
    afid: 123
    http-port: 15123
```

It defines the port 15123 and the afid 123 for the target `main`


## Example

With the file **manifest.yml** and **meta.yml** below,
the `afm-translate manifest.yml meta.yml > output`
produces **output** when the standard template
(afb-app-manager version 12.3.0) is used.

### manifest.yml

```
rp-manifest: 1
id: spawn-binding
version: 2.0.0
name: spawn-binding
description: Sandboxed execution of predefined commands
author: Iot-Team <support@redpesk.bzh>
license: GPL3.0

targets:
  - target: main
    content:
      src: lib/spawn-binding.so
      type: application/vnd.redpesk.resource
    required-permission:
      - urn:AGL:permission::partner:scope-platform
    provided-binding:
      - name: spawn
        value: lib/spawn-binding.so

file-properties:
  - name: lib/spawn-binding.so
    value: public
```

### meta.yml

```
"#metadata":
  root-dir: $HOME/app-data/useful
  install-dir: /usr/redpesk/useful
  icons-dir: /usr/redpesk/useful
  redpak: false
  redpak-id:

"#metatarget":
  main:
    afid: 123
    http-port: 15123
```

### output

```
%begin systemd-unit
# auto generated by redpesk framework for spawn-binding version 2.0.0 target main of spawn-binding
%nl
%systemd-unit system
%systemd-unit service afm-appli-spawn-binding--main
[Unit]
Description=Sandboxed execution of predefined commands
X-AFM-description=Sandboxed execution of predefined commands
X-AFM-name=
X-AFM-shortname=
X-AFM-id=spawn-binding
X-AFM-version=2.0.0
X-AFM-author=
X-AFM-author-email=
X-AFM-width=
X-AFM-height=
X-AFM--ID=123
X-AFM--rootdir=$HOME/app-data/useful
X-AFM--wgtdir=$HOME/app-data/useful/usr/redpesk/useful
X-AFM--workdir=$HOME/app-data/useful/var/scope-platform/spawn-binding
X-AFM--target-name=main
X-AFM--content=lib/spawn-binding.so
X-AFM--type=application/vnd.redpesk.resource
X-AFM--visibility=hidden
%nl
X-AFM--scope=platform
After=afm-system-setup.service
After=Network.target
# Adds check to smack or selinux
ConditionSecurity=|smack
ConditionSecurity=|selinux
%nl
# Automatic bound to required api
%nl
[Service]
EnvironmentFile=-$HOME/app-data/useful/CONFDIR/afm/unit.env.d/*
EnvironmentFile=-$HOME/app-data/useful/CONFDIR/afm/widget.env.d/spawn-binding/*
SmackProcessLabel=App:spawn-binding
SELinuxContext=system_u:system_r:spawn_binding_t:s0
SuccessExitStatus=0 SIGKILL
UMask=0077
#DynamicUser=true
User=daemon
Group=nobody
Slice=platform.slice
CapabilityBoundingSet=
ExecStartPre=- /bin/mkdir -p $HOME/app-data/useful/var/scope-platform/spawn-binding
SystemCallFilter=~@clock
%nl
Environment=AFM_ID=spawn-binding
Environment=AFM_APP_INSTALL_DIR=$HOME/app-data/useful/usr/redpesk/useful
Environment=PATH=/usr/sbin:/usr/bin:/sbin:/bin:$HOME/app-data/useful/usr/redpesk/useful/bin
Environment=LD_LIBRARY_PATH=$HOME/app-data/useful/usr/redpesk/useful/lib
Environment=AFM_WORKDIR=$HOME/app-data/useful/var/scope-platform/spawn-binding
Environment=AFM_WSAPI_DIR=$HOME/app-data/useful/$HOME/app-data/useful/run/platform/apis/ws
Environment=XDG_DATA_HOME=$HOME/app-data/useful/var/scope-platform/spawn-binding
Environment=XDG_CONFIG_HOME=$HOME/app-data/useful/var/scope-platform/spawn-binding
Environment=XDG_CACHE_HOME=$HOME/app-data/useful/var/scope-platform/spawn-binding
Environment=XDG_RUNTIME_DIR=$HOME/app-data/useful/$HOME/app-data/useful/run/platform
SyslogIdentifier=afbd-spawn-binding
StandardInput=null
StandardOutput=journal
StandardError=journal
Type=oneshot
ExecStart=/bin/true
%end systemd-unit
```
