# The permissions

## Permission's names

The proposal here is to specify a naming scheme for permissions
that allows the system to be as stateless as possible.
The current specification includes in the naming of permissions either
the name of the bound binding when existing and the level of the
permission itself.
Doing this, there is no real need for the
framework to keep installed permissions in a database.

The permission names are [URN][URN] of the form:

```bash
    urn:AGL:permission:<api>:<level>:<hierarchical-name>
```

where "AGL" is the NID (the namespace identifier) dedicated to AGL.
(note: a RFC should be produced to standardize this name space)

The permission names are made of NSS (the namespace specific string)
starting with "permission:" and followed by colon separated
fields.
The 2 first fields are `<api>` and `<level>` and the remaining
fields are grouped to form the `<hierarchical-name>`.

```bash
    <api> ::= [ <pname> ]

    <pname> ::= 1*<pchars>

    <pchars> ::= <upper> | <lower> | <number> | <extra>

    <extra> ::= "-" | "." | "_" | "@"
```

The field `<api>` can be made of any valid character for NSS except
the characters colon and star (:*).
This field designates the api providing the permission.
This scheme is used to deduce binding requirements
from permission requirements.
The field `<api>` can be the empty string when the permission
is defined by the system itself.

[PROPOSAL 1] The field `<api>` if starting with the character "@" represents
a transversal/cross permission not bound to any binding.

[PROPOSAL 2]The field `<api>` if starting with the 2 characters "@@"
in addition to a permission not bound to any binding, represents a
permission that must be set at installation and that can not be
revoked later.

    <level> ::= 1*<lower>

The field `<level>` is made only of letters in lower case.
The field `<level>` can only take some predefined values:

- system
- platform
- partner
- tiers
- owner
- public

The field `<hierarchical-name>` is made of `<pname>` separated
by colons.

    <hierarchical-name> ::= <pname> 0*(":" <pname>)

The names at left are hierarchically grouping the
names at right.
This hierarchical behaviour is intended to
be used to request permissions using hierarchical grouping.

## Permission value

In some case, it could be worth to add a value to a permission.

Currently, the framework allows it for permissions linked to
systemd.
But this not currently used.

Conversely, permissions linked to cynagora can't carry data
except in their name.

Thus to have a simple and cleaner model, it is better to forbid
attachment of value to permission.

## Example of permissions

Here is a list of some possible permissions.
These permissions are available the 21th of May 2019.

- urn:AGL:permission::platform:no-oom
  Set OOMScoreAdjust=-500 to keep the out-of-memory
  killer away.
- urn:AGL:permission::partner:real-time
  Set IOSchedulingClass=realtime to give to the process
  realtime scheduling.
  Conversely, not having this permission set RestrictRealtime=on
  to forbid realtime features.
- urn:AGL:permission::public:display
  Adds the group "display" to the list of supplementary groups
  of the process.
- urn:AGL:permission::public:syscall:clock
  Without this permission SystemCallFilter=~@clock is set to
  forfid call to clock.
- urn:AGL:permission::public:no-htdocs
  The http directory served is not "htdocs" but "."
- urn:AGL:permission::public:applications:read
  Allows to read data of installed applications (and to
  access icons).
- urn:AGL:permission::partner:service:no-ws
  Forbids services to provide its API through websocket.
- urn:AGL:permission::partner:service:no-dbus
  Forbids services to provide its API through D-Bus.
- urn:AGL:permission::system:run-by-default
  Starts automatically the application. Example: home-screen.
- urn:AGL:permission::partner:scope-platform
  Install the service at the scope of the platform.
- urn:AGL:permission::system:capability:keep-all
  Keep all capabilities for the service. Note that implementing
  that permission is not mandatory or can be adapted for the given
  system.
- `http://tizen.org/privilege/internal/dbus`
  Permission to use D-Bus.

[URN]: https://tools.ietf.org/rfc/rfc2141.txt "RFC 2141: URN Syntax"