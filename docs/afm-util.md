#  Using afm-util

The command line tool **afm-util** uses *afb-client*
to send orders to **afm-system-daemon**.

The syntax is simple: `afm-util [option]... command [argument]`

- it accepts a command and when required, an attached argument;

- the option **--uid UID** (or **-u UID**), if permitted,
  allows to play the command for an other user;

  Required permission: *urn:AGL:permission:afm:system:set-uid*

- the option **--all** (or **-a**), if permitted, allows to show
  system information otherwise hidden.

  Required permission: *urn:AGL:permission:afm:system:widget*
  or *urn:AGL:permission:afm:system:widget:view-all*


Here is how to list installed applications using **afm-util**:

```bash
> afm-util runnables
```

## commands

### afm-util runnables

Synopsis: `afm-util [--uid UID] [--all] runnables`

Lists the runnable widgets installed

Required permission: *urn:AGL:permission:afm:system:widget*
or *urn:AGL:permission:afm:system:widget:detail*

### afm-util detail

Synopsis: `afm-util [--uid UID] detail id`

Prints detail about the installed widget of id

Required permission: *urn:AGL:permission:afm:system:widget*
or *urn:AGL:permission:afm:system:widget:detail*

### afm-util start

Synopsis: `afm-util [--uid UID] start id`

Starts an instance of the widget of id

Required permission: *urn:AGL:permission:afm:system:widget*
or *urn:AGL:permission:afm:system:widget:start*

### afm-util runners

Synopsis: `afm-util [--uid UID] [--all] runners`

Lists the running instance

Required permission: *urn:AGL:permission:afm:system:runner*
or *urn:AGL:permission:afm:system:runner:state*

### afm-util state

Synopsis: `afm-util [--uid UID] state rid`

Gets status of the running instance rid

Required permission: *urn:AGL:permission:afm:system:runner*
or *urn:AGL:permission:afm:system:runner:state*

### afm-util terminate

Synopsis: `afm-util [--uid UID] terminate rid`

Terminates the running instance rid

Required permission: *urn:AGL:permission:afm:system:runner*
or *urn:AGL:permission:afm:system:runner:kill*


## Command aliases

For historical and practical reasons, most commands have alias.
Here is the list

| official name | alias name |
|:--------------|:-----------|
| runnables     | list       |
| detail        | info       |
| runners       | ps         |
| start         | run, once  |
| terminate     | kill       |
| state         | status     |
| help          | -h, --help |


