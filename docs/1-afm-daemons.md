# The application framework

## Introduction

The daemon ***afm-system-daemon*** handle applications life.
Understand that they will manage operations that mainly are:

- ***running***
- ***terminating***
- ***inventory***

In addition, they ensure that operations use the security framework as needed
and that applications are executed in the correct context.

The daemon ***afm-system-daemon*** is accessible through redpesk
micro-service architecture using either the binder ***afb-binder*** or
the client library ***libafbcli***.

## Starting **afm-system-daemon**

***afm-system-daemon*** is started by systemd services.
Service files are generally located in the directory
*/usr/lib/systemd/system/afm-system-daemon.service*.

Internally, the daemon is built as a binding served by afb-binder.

## Tasks of **afm-system-daemon**

### Maintaining list of applications

At start **afm-system-daemon** scans the directories containing
applications and load in memory a list of available applications
accessible by current user.

When **afm-system-daemon** installs or removes an application,
on success it sends the signal.
When receiving such a signal, **afm-system-daemon** rebuilds its
applications list.

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
