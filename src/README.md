
Summary of source files
=======================

afm-db.[ch]
-----------

Handles a set of directories as a set of applications.
Builds in memory the list of the applications.


afm-launch.[ch]
---------------

Launches applications.
Uses the descriptions in /etc/afm to know how to launch
applications based on their types.


afm-launch-mode.[ch]
--------------------

Simple file for managing the known launch modes.

afm-run.[ch]
------------

Manages the live of running applications: starts it, stop it,
continue it, terminates it. Manages the list of running
applications.

Applications are handled as process groups. It allows to use
applications that fork or the binder/client division.

afm-system-daemon.[ch]
----------------------

afm-user-daemon.[ch]
--------------------

User daemon serving on the D-Bus of the session.

wgt.[ch]
--------

Access to files of a widget.

Implements the mechanism for seeking for files using the locale settings. 



