#  Using ***afm-util***

The command line tool **afm-util** uses *afb-client*
to send orders to **afm-system-daemon**.

It allows to send command to ***afm-system-daemon*** either
interactively at shell prompt or in a script.

The syntax is simple:

- it accepts a command and when required, an attached arguments;
- the option **--uid UID** (or **-u UID**) allows authorized person
  (root) to play the command for an other user
- the option **--all** (or **-a**) shows also system applications.


Here is the summary of ***afm-util***:

- **afm-util [--uid UID] [--all] runnables**:
  list the runnable widgets installed

- **afm-util [--uid UID] detail id**:
  print detail about the installed widget of id

- **afm-util [--uid UID] [--all] runners**:
  list the running instance

- **afm-util [--uid UID] start id**:
  start an instance of the widget of id

- **afm-util [--uid UID] once id**:
  run once an instance of the widget of id

- **afm-util [--uid UID] terminate rid**:
  terminate the running instance rid

- **afm-util [--uid UID] state rid**:
  get status of the running instance rid

Here is how to list applications using ***afm-util***:

```bash
    afm-util runnables
```

