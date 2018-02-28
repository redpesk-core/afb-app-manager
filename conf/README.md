Configuration of af-main using systemd
======================================

Mechanism to start user sessions
--------------------------------

The mechanism to start a session for the user of **UID** is
to start the service **afm-user-session@UID.service**.

This has the effect of starting a session.

To achieve that goal the first is to start the user session.
This is done using the 2 systemd directives [1]:

    User=%i
    PAMName=afm-user-session

The first tells what is the user. %i is replaced by the parameter
of the service: UID. So the user is referenced here by its number.

For this user, the PAM script **afm-user-session** is evaluated.
It is implmented by the file */etc/pam.d/afm-user-session*.
That script MUST refer to *pam_systemd.so* for opening the session
with systemd. It often takes the form of a line of the form:

    session     optional      pam_systemd.so

that is directly or indirectly (through includes) activated by
**afm-user-session**. [2] [3]

The effect of starting a systemd user session is to start the
user services and the most important one: dbus.

When the user session is started, the service
**afm-user-session@UID.service** enters its second phase:
activation of the user session for the framework.

This is achieved by activating the target **afm-user-session@.target**.
But activating a *system* unit from a *user* session is a
thing that has to be safe. This is done by the program
**afm-user-session**. This program runs as rot (with the set-uid)
and simply execute *systemctl --wait start afm-user-session@UID.target*.
Where *UID* is the user id of the calling process.

[1] https://www.freedesktop.org/software/systemd/man/systemd.exec.html
[2] https://www.freedesktop.org/software/systemd/man/pam_systemd.html
[3] https://linux.die.net/man/5/pam.conf
