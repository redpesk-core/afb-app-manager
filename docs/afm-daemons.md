# The application framework

## Introduction

The daemon ***afmpkg-daemon*** installs and removes applications.
It is automatically called when ***dnf*** installs or removes applications.

The daemon ***afm-system-daemon*** handles life of installed application:

- ***running***
- ***terminating***
- ***inventory***

In addition, they ensure that operations use the security framework as needed
and that applications are executed in the correct context.

The daemon **afm-system-daemon** is accessible through redpesk
micro-service architecture using either the binder **afb-binder**,
the client library **libafbcli** or the programs **afb-client** and
**afm-util**.

## Starting **afm-system-daemon**

**afm-system-daemon** is started by systemd services.

Internally, the daemon is built as a binding served by afb-binder.

## Tasks of **afm-system-daemon**

### Maintaining list of applications

At start **afm-system-daemon** scans the directories containing
applications and load in memory a list of available applications
accessible by current user.

**afm-system-daemon** provides the data it collects about
applications to its clients.
Clients may either request the full list
of available applications or a more specific information about a
given application.

### Launching application

**afm-system-daemon** launches application by using systemd.
Systemd builds a secure environment for the application
before starting it.

Once launched, running instances of application receive
a runid that identify them. On previous versions, the *runid*
had a special meaning. The current version uses the linux *PID*
of the launched process as *runid*.

### Managing instances of running applications

**afm-system-daemon** manages the list of applications
that it launched.

When owning the right permissions, a client can get the list
of running instances and details about a specific
running instance.
It can also terminate a given application.

## Starting **afmpkg-daemon**

**afmpkg-daemon** is started by systemd services on need.

## Tasks of **afmpkg-daemon**

The daemon ***afmpkg-daemon*** is activated by the *dnf*'s plugin
named ***redpesk*** when it detects that the installed or removed
package is an afmpkg.

When ***afmpkg-daemon*** installs or removes an application,
it sends a signal to **afm-system-daemon** that updates its
applications database.

### Installing applications

***afmpkg-daemon*** reads the manifest of the installed package
check it and, according to its content, set up the system security
with the help of the security manager.

### Uninstalling applications

***afmpkg-daemon*** contacts the serity manager to cleanup the
security rules for the removed application and to remove their
security setup.