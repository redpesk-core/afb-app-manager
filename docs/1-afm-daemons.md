# The application framework

## Introduction

The daemon ***afm-system-daemon*** handle applications life.
Understand that they will manage operations that mainly are:

- ***installation***
- ***uninstallation***
- ***running***
- ***terminating***
- ***inventory***

In addition, they ensure that operations use the security framework as needed
and that applications are executed in the correct context.

The daemon ***afm-system-daemon*** is accessible through AGL
micro-service architecture using either the binder ***afb-binder*** or
the client library ***libafbwsc***.

## Starting **afm-system-daemon**

***afm-system-daemon*** is launched by systemd services.
Normally, service files are located in the directory
*/lib/systemd/system/afm-system-daemon.service*.

Internally, the daemon is built as a binding served by afb-daemon.

## Tasks of **afm-system-daemon**

### Maintaining list of applications

At start **afm-system-daemon** scans the directories containing
applications and load in memory a list of available applications
accessible by current user.

When **afm-system-daemon** installs or removes an application,
on success it sends the signal **.
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

### Installing and uninstalling applications

If the client own the right permissions,
**afm-system-daemon** delegates that task
to **afm-system-daemon**.

## Using ***afm-util***

The command line tool ***afm-util*** is available in devel mode.

It uses afb-client-demo to send orders to **afm-system-daemon**.
This small scripts allows to send command to ***afm-system-daemon*** either
interactively at shell prompt or scriptically.

The syntax is simple:

- it accept a command and when requires attached arguments.

Here is the summary of ***afm-util***:

- **afm-util runnables      **:
  list the runnable widgets installed

- **afm-util install    wgt **:
  install the wgt file

- **afm-util uninstall  id  **:
  remove the installed widget of id

- **afm-util detail     id  **:
  print detail about the installed widget of id

- **afm-util runners        **:
  list the running instance

- **afm-util start      id  **:
  start an instance of the widget of id

- **afm-util once      id  **:
  run once an instance of the widget of id

- **afm-util terminate  rid **:
  terminate the running instance rid

- **afm-util state      rid **:
  get status of the running instance rid

Here is how to list applications using ***afm-util***:

```bash
    afm-util runnables
```

[afm-daemons]: pictures/afm-daemons.svg
