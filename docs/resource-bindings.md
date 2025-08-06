Resource bindings
=================

Some bindings are like libraries for programs and
are not doing anything by themselves but are needing
some extra elements like a configuration.

A typical example is the MODBUS binding. That binding
requires a configuration file describing connected
hardwares.

Because bindings and configurations are coming from
different sources and are developed by different
teams, there is a need for guidelines on how to assemble
them.

The redpesk framework gives flexibility on how to
implement such assembly. This document presents
the recommended solution(s) to solve the problem.

## Basic usage of a resource bindings

The resource bindings are bindings that are exported
to third parties. It is in the framework since 2018.
To achieve it the manifest file must include the
`provided-binding` declaration, as on the example:

```yaml
targets:
  - target: main
    content:
      src: lib/extra-binding.so
      type: application/vnd.redpesk.resource
provided-binding:
  - name: extra
    value: lib/extra-binding.so
file-properties:
   - name: lib/extra-binding.so
     value: public
```

On that example, the resource binding `lib/extra-binding.so`
(a shared library) is exported under the name `extra`.
Note well that the provided binding must be exported
publicly using the file-properties section.

Such provided bindings can be imported by applications
by requiring it in the manifest. This is achieved by
adding `required-binding` to the target that requires it,
as on the below example:

```yaml
targets:
  - target: main
    content:
      src: .rpconfig/manifest.yml
      type: application/vnd.redpesk.service
    required-binding:
      - name: extra
        value: extern
```

On that example, the service imports the binding exported
as extra.

A resource binding with no further configuration than shown
here is run as a user binding and can only be used by bindings
of the same user. A resource binding installed platform-wide
(with the `urn:redpesk:permission::partner:scope-platform` permission)
can be used by bindings from all users and by other platform-wide
bindings.

## Using a resource binding with a config

When a service imports a binding and when that binding
expect a configuration specific to the service, the
service must provide a configuration.

This is done in two steps:

- tell through manifest file to use a config file
- in that config file, tell the binding what is its configuration

Since version 12.2.4, the framework allows to tell
what configuration files are to required to be loaded.
This is done using `required-config` at target level,
as on the below example:

```yaml
targets:
  - target: main
    content:
      src: .rpconfig/manifest.yml
      type: application/vnd.redpesk.service
    required-binding:
      - name: extra
        value: extern
    required-config:
      - etc/config-extra-common.yml
      - etc/config-extra-extra.yml
```

On that example, the service requires to load 2 configurations.

When more that one configuration is given, the configuration are
merged together to form the final configuration.

Such given configuration are equivalent to giving options on the
binder command lines. Then for setting the configuration of a binding
it is the afb-binder's option `--set`. Here is the manual extract of
this option:

```man
   -s, --set VALUE
       Set parameters values for APIs.  The  value  can  be  of  the  form
       [API]/[KEY]:JSON or {"API":{"KEY":JSON},...}
```

So as explained in the manual, the configuration file should look
as below:

```json
{
   "set": {
      "API": {
         ...
      }
   }
}
```

Here caution has to be taken because two kinds of bindings exist:

- when the binding dynamically creates APIs without using [afb_binding_t][1]
  static description and when the API name depends of the configuration,
  in that case, the API name is the basename of the binding library must
  be used (example: if the binding is `lib/hello.so` then the API is `hello.so`).

- In other cases, this is the usual API name;



[1]: https://docs.redpesk.bzh/docs/en/master/developer-guides/reference-v4/types-and-globals.html#the-type-afb_binding_t

