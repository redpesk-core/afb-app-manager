# Installation within redpesk for dnf, RPM and redpak

Installation for the framework is made during normal
installation using dnf (*).

When installing or uninstalling:
* use `dnf` or `rpm` (possibly as simple user within rednode)
* the RPM plugin for redpesk checks and detects if the (un)installed
  package is integrated in the framework (it detects files
  `config.xml` or `.rpconfig/manifest.yml`)
* if it is a package managed by the framework, the RPM plugin for
  redpesk contacts the daemon afmpkg-installer (service afmpkg-installer)
* `afmpkg-installer` scan the manifest files in order to set or remove
  service files, permissions and secirity labels
* `afmpkg-installer` contacts `sec-lsm-manager` to setup permisions
  and labels

Before calling `rpm` or `dnf`, the following environment variables
can be set. They will be passed to `afmpkg-installer`:

* AFMPKG_REDPAKID: this is transmitted to configuration setup in order
  to correctly set the services

* AFMPKG_TRANSID: this identifies the transaction. Set it if you want
  to get a real (un)installation status using `afmpkg-status`.



(*) Issue to be considered: installation during image construction
when the framework can't be contacted by network.
Idea:
```nomnoml
[<choice>server is here?]
[<choice>is root?]
[<box>normal flow]
[<box>special flow]
[<box>fail!]

[server is here?] yes ->  [normal flow]
[server is here?] no  ->  [is root?]
[is root?] no -> [fail!]
[is root?] yes -> [special flow]
```
