
The afm-user-daemon
===================

    version: 1
    Date:    15 March 2016
    Author:  José Bollo


Foreword
--------

This document describes what we intend to do. It may happen that our
current implementation and the content of this document differ.

In case of differences, it is assumed that this document is right
and the implementation is wrong.


Introduction
------------

The daemon **afm-user-daemon** is in charge of handling
applications for one user. Its main tasks are:

 - enumerate the applications that the user can run
   and keep the list avalable on demand

 - start applications for the user, set their running
   environment, set their security context

 - list the current runner applications

 - stop (aka pause), continue (aka resume), terminate
   the running instance of application

 - transfer requests for installation or uninstallation
   of applications to the dedicated system daemon
   **afm-system-daemon**

The **afm-user-daemon** takes its orders from the session
instance of D-Bus.

The figure below summarizes the situation of the
**afm-user-daemon** in the system.

    +------------------------------------------------------------+
    |                          User                              |
    |                                 +---------------------+    |
    |     +---------------------+     |   Smack isolated    |    |
    |     |   D-Bus   session   +     |    APPLICATIONS     |    |
    |     +----------+----------+     +---------+-----------+    |
    |                |                          |                |
    |                |                          |                |
    |     +----------+--------------------------+-----------+    |
    |     |                                                 |    |
    |     |                  afm-user-daemon                |    |
    |     |                                                 |    |
    |     +----------+----------------------+----------+----+    |
    |                |                      |          :         |
    |                |                      |          :         |
    :================|======================|==========:=========:
    |                |                      |          :         |
    |     +----------+----------+     +-----+-----+    :         |
    |     |   D-Bus   system    +-----+  CYNARA   |    :         |
    |     +----------+----------+     +-----+-----+    :         |
    |                |                      |          :         |
    |     +----------+---------+    +-------+----------+----+    |
    |     | afm-system-daemon  +----+   SECURITY-MANAGER    |    |
    |     +--------------------+    +-----------------------+    |
    |                                                            |
    |                          System                            |
    +------------------------------------------------------------+


Tasks of **afm-user-daemon**
----------------------------

### Maintaining list of applications ###

At start **afm-user-daemon** scans the directories containing
the applications and load in memory the list applications
availables to the current user.

When **afm-system-daemon** installs or removes an application,
it sends the signal *org.AGL.afm.system.changed* on success.
If it receives that signal, **afm-user-daemon** rebuild its
list of applications.

**afm-user-daemon** provides the data that it collected about
application to its clients that either want to get that list
or to get information about one application.

### Launching applications ###

**afm-user-daemon** launchs the applications. This means
that its builds a secure environment for the application
and then start it inside that secured environment.

Applications of different kind can be launched.

This is set using a configuration file that describes
how to launch an application of a given kind for a given
mode.

There is two launching modes: local or remote.

Launching an application locally means that
the application and its binder are launcher together.

Launching application remotely means that only the
binder is launched for the application.

Once launched, running instances of application receive
a runid that identify them.

### Managing instances of running applications ###

**afm-user-daemon** manages the list of applications
that it launched.

With the good permissions, a client can get the list
of the running instances and details about a specific
running instance. It can also terminate, stop or
continue a given application.

### Installing and uninstalling applications ###

If the client has the good permission,
**afm-user-daemon** delegates that task
to **afm-system-daemon**.


Starting **afm-user-daemon**
-----------------------------

**afm-user-daemon** is launched as a **systemd** service
attached to user sessions. Normally, the service file is
located at /usr/lib/systemd/user/afm-user-daemon.service.

The options for launching **afm-user-daemon** are:

    -a
    --application directory
    
         Includes the given application directory to
         the database base of applications.
    
         Can be repeated.
    
    -r
    --root directory
    
         Includes the root application directory to
         the database base of applications.

         Note that the default root directory for
         applications is always added. It is defined
         to be /usr/share/afm/applications (may change).
    
         Can be repeated.
    
    -m
    --mode (local|remote)
    
         Set the default launch mode.
         The default value is 'local'
    
    -d
    --daemon
    
         Daemonizes the process. It is not needed by sytemd.
    
    -q
    --quiet
    
         Reduces the verbosity (can be repeated).
    
    -v
    --verbose
    
         Increases the verbosity (can be repeated).
    
    -h
    --help
    
         Prints a short help.
    

Configuration of the launcher
-----------------------------

It contains rules for launching applications.
When **afm-user-daemon** need to launch an application,
it looks to the mode of launch, local or remote, and the
type of the application as given by the file ***config.xml***
of the widget.

This couple mode and type allows to select the rule.

The configuration file is **/etc/afm/afm-launch.conf**.

It contains sections and rules. It can also contain comments
and empty lines to improve the readability.

The separators are space and tabulation, any other character
is meaning something.

The format is line oriented.
The new line character separate the lines.

Lines having only separators are blank lines and are skipped.
Line having the character # (sharp) as first not separator character
are comment lines and are ignored.

Lines starting with a not separator character are differents
of lines starting with a separator character.

The grammar of the configuration file is defined below:

    CONF: *COMMENT *SECTION
    
    SECTION: MODE *RULE
    
    RULE: +TYPE VECTOR ?VECTOR
    
    MODE: 'mode' +SEP ('local' | 'remote') *SEP EOL
    
    TYPE: DATA *SEP EOL
    
    VECTOR: +SEP DATA *(+SEP NDATA) *SEP EOL
    
    DATA: CHAR *NCHAR
    NDATA: +NCHAR

    EOL: NL *COMMENT
    COMMENT: *SEP CMT *(SEP | NCHAR) NL

    NL: '\x0a'
    SEP: '\x20' | '\x09'
    CMT: '#'
    CHAR: '\x00'..'\x08' | '\x0b'..'\x1f' | '\x21' | '\x22' | '\x24'..'\xff'
    NCHAR: CMT | CHAR
    
Here is a sample of configuration file for defining how
to launch an application declared of types *application/x-executable*,
*text/x-shellscript* and *text/html* in mode local:

    mode local
    
    application/x-executable
    text/x-shellscript
        %r/%c
    
    text/html
        /usr/bin/afb-daemon --mode=local --readyfd=%R --alias=/icons:%I --port=%P --rootdir=%r --token=%S --sessiondir=%D/.afb-daemon
        /usr/bin/web-runtime http://localhost:%P/%c?token=%S

This shows that:

 - within a section, several rules can be defined
 - within a rule, several types can be defined
 - within a rule, one or two vectors can be defined
 - vectors are using %substitution
 - launched binaries must be defined with their full path

### mode local

Within this mode, the launchers have either one or two vectors
describing them. All of these vectors are treated as programs
and are executed with the system call 'execve'.

The first vector is the leader vector and it defines the process
group. The second vector (if any) is attached to the group
defined by this first vector.

### mode remote

Within this mode, the launchers have either one or two vectors
describing them.

The first vector is treated as a program and is executed with
the system call 'execve'.

The second vector (if any) defines a text that is returned
to the caller. This mechanism can be used to return the uri
to connect to for executing the application remotely.

The daemon ***afm-user-daemon*** allocates a port for the
running the application remotely.
The current implmentation of the port allocation is just
incremental.
A more reliable (cacheable and same-originable) allocation
is to be defined.

### %substitutions

Vectors can include sequences of 2 characters that have a special
meaning. These sequences are named *%substitution* because their
first character is the percent sign (%) and because each occurrence
of the sequence is replaced, at launch time, by the value associated
to sequences.

Here is the list of *%substitutions*:

 - ***%%***: %.

   This simply emits the percent sign %

 - ***%a***: appid

   This is the application Id of the launched application.

   Defined by the attribute **id** of the element **<widget>**
   of **config.xml**.

 - ***%c***: content

   The file within the widget directory that is the entry point.

   For a HTML application, it is the relative path to the main
   page (aka index.html).

   Defined by the attribute **src** of the element **<content>**
   of **config.xml**.

 - ***%D***: datadir

   Path of the directory where the application runs (cwd)
   and stores its data.

   It is equal to %h/%a.

 - ***%H***: height

   Requested height for the widget.

   Defined by the attribute **height** of the element **<widget>**
   of **config.xml**.

 - ***%h***: homedir

   Path of the home directory for all applications.

   It is generally equal to $HOME/app-data

 - ***%I***: icondir

   Path of the directory were the icons of the applications can be found.

 - ***%m***: mime-type

   Mime type of the launched application.

   Defined by the attribute **type** of the element **<content>**
   of **config.xml**.

 - ***%n***: name

   Name of the application as defined by the content of the
   element **<name>** of **config.xml**.

 - ***%p***: plugins

   Unhandled until now.

   Will be the colon separated list of plugins and plugins directory.

 - ***%P***: port

   A port to use. It is currently a kind of random port. The precise
   model is to be defined later.

 - ***%R***: readyfd

   Number of the file descriptor to use for signalling
   readyness of the launched process.

 - ***%r***: rootdir

   Path of the directory containing the widget and its data.

 - ***%S***: secret

   An hexadecimal number that can be used to pair the client
   with its server binder.

 - ***%W***: width

   Requested width for the widget.

   Defined by the attribute **width** of the element **<widget>**
   of **config.xml**.


The D-Bus interface
-------------------

### Overview of the dbus interface

***afm-user-daemon*** takes its orders from the session instance
of D-Bus. The use of D-Bus is great because it allows to implement
discovery and signaling.

The dbus of the session is by default adressed by the environment
variable ***DBUS_SESSION_BUS_ADDRESS***. Using **systemd** 
the variable *DBUS_SESSION_BUS_ADDRESS* is automatically set for
user sessions.

The **afm-user-daemon** is listening with the destination name
***org.AGL.afm.user*** at the object of path ***/org/AGL/afm/user***
on the interface ***org.AGL.afm.user*** for the below detailed
members ***runnables***, ***detail***, ***start***, ***terminate***,
***stop***, ***continue***, ***runners***, ***state***,
***install*** and ***uninstall***.

D-Bus is mainly used for signaling and discovery. Its optimized
typed protocol is not used except for transmitting only one string
in both directions.

The client and the service are using JSON serialisation to
exchange data. 

The D-Bus interface is defined by:

 * DESTINATION: **org.AGL.afm.user**

 * PATH: **/org/AGL/afm/user**

 * INTERFACE: **org.AGL.afm.user**

The signature of any member of the interface is ***string -> string***
for ***JSON -> JSON***.

This is the normal case. In case of error, the current implmentation
returns a dbus error that is a string.

Here is an example that use *dbus-send* to query data on
installed applications.

    dbus-send --session --print-reply \
        --dest=org.AGL.afm.user \
        /org/AGL/afm/user \
        org.AGL.afm.user.runnables string:true

### Using ***afm-util***

The command line tool ***afm-util*** uses dbus-send to send
orders to **afm-user-daemon**. This small scripts allows to
send command to ***afm-user-daemon*** either interactively
at shell prompt or scriptically.

The syntax is simple: it accept a command and if the command
requires it, the argument to the command.

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

 - **afm-util terminate  rid **:

   terminate the running instance rid

 - **afm-util stop       rid **:

   stop the running instance rid

 - **afm-util continue   rid **:

   continue the previously rid

 - **afm-util state      rid **:

   get status of the running instance rid


Here is how to list applications using ***afm-util***:

    afm-util runnables

---

### The protocol over D-Bus

Recall:

 * **DESTINATION**: org.AGL.afm.user

 * **PATH**: /org/AGL/afm/user

 * **INTERFACE**: org.AGL.afm.user

---

#### Method org.AGL.afm.user.detail

**Description**: Get details about an application from its id.

**Input**: the id of the application as below.

Either just a string:

    "appli@x.y"

Or an object having the field "id" of type string:

    {"id":"appli@x.y"}

**Output**: A JSON object describing the application containing
the fields described below.

    {
      "id":          string, the application id (id@version)
      "version":     string, the version of the application
      "width":       integer, requested width of the application
      "height":      integer, resqueted height of the application
      "name":        string, the name of the application
      "description": string, the description of the application
      "shortname":   string, the short name of the application
      "author":      string, the author of the application
    }

---

#### Method org.AGL.afm.user.runnables

**Description**: Get the list of applications that can be run.

**Input**: any valid json entry, can be anything except null.

**output**: An array of description of the runnable applications.
Each item of the array contains an object containing the detail of
an application as described above for the method
*org.AGL.afm.user.detail*.

---

#### Method org.AGL.afm.user.install

**Description**: Install an application from its widget file.

If an application of the same *id* and *version* exists, it is not
reinstalled except if *force=true*.

Applications are installed in the subdirectories of the common directory
of applications.
If *root* is specified, the application is installed under the
sub-directories of the *root* defined.

Note that this methods is a simple accessor to the method
***org.AGL.afm.system.install*** of ***afm-system-daemon***.

After the installation and before returning to the sender,
***afm-user-daemon*** sends the signal ***org.AGL.afm.user.changed***.

**Input**: The *path* of the widget file to install and, optionaly,
a flag to *force* reinstallation, and, optionaly, a *root* directory.

Either just a string being the absolute path of the widget file:

    "/a/path/driving/to/the/widget"

Or an object:

    {
      "wgt": "/a/path/to/the/widget",
      "force": false,
      "root": "/a/path/to/the/root"
    }

"wgt" and "root" must be absolute paths.

**output**: An object with the field "added" being the string for
the id of the added application.

    {"added":"appli@x.y"}

---

#### Method org.AGL.afm.user.uninstall

**Description**: Uninstall an application from its id.


Note that this methods is a simple accessor to the method
***org.AGL.afm.system.uninstall*** of ***afm-system-daemon***.

After the uninstallation and before returning to the sender,
***afm-user-daemon*** sends the signal ***org.AGL.afm.user.changed***.

**Input**: the *id* of the application and, otpionaly, the path to
*root* of the application.

Either a string:

    "appli@x.y"

Or an object:

    {
      "id": "appli@x.y",
      "root": "/a/path/to/the/root"
    }

**output**: the value 'true'.

---

#### Method org.AGL.afm.user.start

**Description**:

**Input**: the *id* of the application and, optionaly, the
start *mode* as below.

Either just a string:

    "appli@x.y"

Or an object having the field "id" of type string and
optionaly a field mode:

    {"id":"appli@x.y","mode":"local"}

The field "mode" as a string value being either "local" or "remote".

**output**: The *runid* of the application launched.
The runid is an integer.

---

#### Method org.AGL.afm.user.terminate

**Description**: Terminates the application of *runid*.

**Input**: The *runid* (an integer) of the running instance to terminate.

**output**: the value 'true'.

---

#### Method org.AGL.afm.user.stop

**Description**: Stops the application of *runid* until terminate or continue.

**Input**: The *runid* (an integer) of the running instance to stop.

**output**: the value 'true'.

---

#### Method org.AGL.afm.user.continue

**Description**: Continues the application of *runid* previously stopped.

**Input**: The *runid* (an integer) of the running instance to continue.

**output**: the value 'true'.

---

#### Method org.AGL.afm.user.state

**Description**: Get informations about a running instance of *runid*.

**Input**: The *runid* (an integer) of the running instance inspected.

**output**: An object describing the state of the instance. It contains:
the runid (an integer), the id of the running application (a string),
the state of the application (a string being either "starting", "running"
or "stopped").

Example of returned state:

    {
      "runid": 2,
      "state": "running",
      "id": "appli@x.y"
    }

---

#### Method org.AGL.afm.user.runners

**Description**: Get the list of the currently running instances.

**Input**: anything.

**output**: An array of states, one per running instance, as returned by
the methodd ***org.AGL.afm.user.state***.

