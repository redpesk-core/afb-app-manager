## The configuration file afm-unit.conf

The integration of the framework with systemd
mainly consists of creating the systemd unit
files corresponding to the need and requirements
of the installed packages.

This configuration file named `afm-unit.conf` installed
on the system with the path `/etc/afm/afm-unit.conf`
describes how to generate all units from the *config.xml*
configuration files of packages.
The description uses an extended version of the templating
formalism of [mustache][] to describes all the units.

Let present how it works using the following diagram that
describes graphically the workflow of creating the unit
files for systemd `afm-unit.conf` from the configuration
file of the package `config.xml`:

![make-units](pictures/make-units.svg)

In a first step, and because [mustache][] is intended
to work on JSON representations, the configuration file is
translated to an internal JSON representation.
This representation is shown along the examples of the documentation
of the config files of packages.

In a second step, the mustache template `afm-unit.conf`
is instantiated using the C library [mustach][] that follows
the rules of [mustache][mustache] and with all its available
extensions:

- use of colon (:) for explicit substitution
- test of values with = or =!

In a third step, the result of instantiating `afm-unit.conf`
for the package is split in units.
To achieve that goal, the lines containing specific directives are searched.
Any directive occupy one full line.
The directives are:

- %nl
  Produce an empty line at the end
- %begin systemd-unit
- %end systemd-unit
  Delimit the produced unit, its begin and its end
- %systemd-unit user
- %systemd-unit system
  Tells the kind of unit (user/system)
- %systemd-unit service NAME
- %systemd-unit socket NAME
  Gives the name and type (service or socket) of the unit.
  The extension is automatically computed from the type
  and must not be set in the name.
- %systemd-unit wanted-by NAME
  Tells to install a link to the unit in the wants of NAME

Then the computed units are then written to the filesystem
and inserted in systemd.

The generated unit files will contain variables for internal
use of the framework.
These variables are starting with `X-AFM-`.
The variables starting with `X-AFM-` but not with `X-AFM--` are
the public variables.
These variables will be returned by the
framework as the details of an application (see **afm-util detail ...**).

Variables starting with `X-AFM--` are private to the framework.
By example, the variable  `X-AFM--http-port` is used to
record the allocated port for applications.
