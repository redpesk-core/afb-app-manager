# Using afm-translate

This document the use of the tiny program `afm-translate`.

At time of writing this documentation, the future of this
command is no clear. It is a valuable program but might evolve
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
each directive occupying one whole line starting with %
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

The command line invocation of `afm-translate` looks like:

```
afm-translate [-t template] mafifest [meta...]
```

where:

- `manifest` is the manifest file of an application, YAML or JSON

- `meta` is one or more file containing the meta data to merge
  to the manifest, YAML or JSON

- `template` is file containing the template to use, MUSTACH.

By default, the template used is the template used
by `afmpkg-installer`

Caution, the metadata are using field name starting with a hash
sign that implies that the keys have to be in quotes if the file
is YAML encoded.

## Example

With the file **manifest.yml** and **meta.yaml** below,
the `afm-translate manifest.yml meta.yaml > output`
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
  redpak: true
  redpak-id:

targets:
    "#metatarget":
        id: useful
        http-port: 15123
```

### output

```
%begin systemd-unit
%nl
%systemd-unit system
%systemd-unit service afm-appli-spawn-binding--@
[Unit]
Description=Sandboxed execution of predefined commands
X-AFM-description=Sandboxed execution of predefined commands
X-AFM-name=
X-AFM-shortname=
X-AFM-id=spawn-binding--
X-AFM-version=2.0.0
X-AFM-author=
X-AFM-author-email=
X-AFM-width=
X-AFM-height=
X-AFM--ID=
X-AFM--redpak-id=0
X-AFM--rootdir=$HOME/app-data/useful
X-AFM--wgtdir=$HOME/app-data/useful//usr/redpesk/useful
X-AFM--workdir=$HOME/app-data/useful//home/%i/app-data/spawn-binding
X-AFM--target-name=
X-AFM--content=
X-AFM--type=
X-AFM--visibility=visible
%nl
X-AFM--scope=user
BindsTo=afm-user-session@%i.target
After=user@%i.service
After=Network.target
ConditionSecurity=|smack
ConditionSecurity=|selinux
%nl
%nl
[Service]
EnvironmentFile=-$HOME/app-data/useful//home/jobol/.locenv/afb/etc/afm/unit.env.d/*
EnvironmentFile=-$HOME/app-data/useful//home/jobol/.locenv/afb/etc/afm/widget.env.d/spawn-binding/*
SmackProcessLabel=App:spawn-binding
SELinuxContext=system_u:system_r:spawn_binding_t:s0
SuccessExitStatus=0 SIGKILL
UMask=0077
User=%i
Slice=user-%i.slice
WorkingDirectory=-$HOME/app-data/useful//home/%i/app-data/spawn-binding
ExecStartPre=/usr/bin/redwrap --redpath $HOME/app-data/useful --  /bin/mkdir -p /home/%i/app-data/spawn-binding
Environment=DBUS_SESSION_BUS_ADDRESS=unix:path=/run/user/%i/bus
CapabilityBoundingSet=
SystemCallFilter=~@clock
%nl
Environment=AFM_ID=spawn-binding--
Environment=AFM_APP_INSTALL_DIR=/usr/redpesk/useful
Environment=PATH=/usr/sbin:/usr/bin:/sbin:/bin:/usr/redpesk/useful/bin
Environment=LD_LIBRARY_PATH=/usr/redpesk/useful/lib
Environment=AFM_WORKDIR=/home/%i/app-data/spawn-binding
Environment=AFM_WSAPI_DIR=/run/user/%i/apis/ws
Environment=XDG_DATA_HOME=/home/%i/app-data/spawn-binding
Environment=XDG_CONFIG_HOME=/home/%i/app-data/spawn-binding
Environment=XDG_CACHE_HOME=/home/%i/app-data/spawn-binding
Environment=XDG_RUNTIME_DIR=/run/user/%i
SyslogIdentifier=afbd-spawn-binding--
StandardInput=null
StandardOutput=journal
StandardError=journal
%end systemd-unit
```
