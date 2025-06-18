# Daemons of the application framework

## Introduction

The application framework of redpesk provides two services running in
background as daemons.
They ensure that operations use correctly the security framework
and that applications are executed in the correct security context.

- **afmpkg-installerd**: this daemon installs and removes applications.
It is automatically called when **dnf** installs or removes applications.
It automatically stops after an inactivity period.

- **afm-system-daemon**: this daemon handles life cycle of installed
  application: listing, starting, stopping. It can be accessed
  through the command line utility *afm-util*.

## afm-system-daemon

The daemon **afm-system-daemon** is accessible through redpesk
micro-service architecture using either the binder **afb-binder**,
the client library **libafbcli** or the programs **afb-client** and
**afm-util**.

It is installed as a systemd service and started automatically on need.

It can also be started, restarted, stopped, checked using `systemctl`
as below:

```bash
$ systemctl status afm-system-daemon
```

### List of applications

At start **afm-system-daemon** scans the directories containing
applications and load in memory a list of available applications
accessible by current user.

**afm-system-daemon** provides the data it collects about
applications to its clients.
Clients may either request the full list
of available applications or a more specific information about a
given application.

### Starting applications

**afm-system-daemon** starts application by using systemd.
Systemd builds a secure environment for the application
before starting it.

Once launched, running instances of application receive
a runid that identify them. On previous versions, the *runid*
had a special meaning. The current version uses the linux *PID*
of the launched process as *runid*.

### List of running applications

**afm-system-daemon** manages the list of applications
that it launched.

When owning the right permissions, a client can get the list
of running instances and details about a specific
running instance.
It can also terminate a given application.


## afmpkg-installerd

The daemon **afmpkg-installerd** is activated by the *dnf*'s plugin
named *redpesk* when it detects that the installed or removed
package is an afmpkg.

After being used, if **afmpkg-installerd** is not used for 5 minutes,
it automatically stops.

### Installing applications

**afmpkg-installerd** reads the metadata of the installed package and
check it.

When metadata are wrong, the installation is cancelled.

Otherwise, when metadata are valid, **afmpkg-installerd** contacts
the *security manager* to setup the system security for the installed
application.

After installing or removing an application, **afmpkg-installerd**
sends to **afm-system-daemon** a signal telling it to update its
applications database.

### Removing applications

**afmpkg-installerd** contacts the *security manager* to cleanup the
security rules for the removed application and to remove their
security setup.
