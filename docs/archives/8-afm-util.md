#  Using ***afm-util***

The command line tool **afm-util** uses *afb-client*
to send orders to **afm-system-daemon**.

It allows to send command to ***afm-system-daemon*** either
interactively at shell prompt or in a script.

The syntax is simple:

- it accepts a command and when required, an attached arguments.

Here is the summary of ***afm-util***:

- **afm-util [--all] runnables**:
  list the runnable widgets installed

- **afm-util install wgt**:
  install the wgt file

- **afm-util uninstall id**:
  remove the installed widget of id

- **afm-util detail id**:
  print detail about the installed widget of id

- **afm-util [--all] runners**:
  list the running instance

- **afm-util start id**:
  start an instance of the widget of id

- **afm-util once id**:
  run once an instance of the widget of id

- **afm-util terminate rid**:
  terminate the running instance rid

- **afm-util state rid**:
  get status of the running instance rid

Here is how to list applications using ***afm-util***:

```bash
    afm-util runnables
```

