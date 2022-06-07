# Application framework of redpesk

## Overview

The application framework of **redpesk**
provides components to install and remove applications
as well as to run them in a secured environment.

The main functionalities are:

- install/remove applications and services

- start/terminate installed applications and services

- answer simple queries: what is installed? what runs?

The application framework fills the gap between the applications
development model and the effective system implementation.

In one hand, there is a programming model that includes security
features through permissions, micro service architecture through
flexible high level API, and, in the other hand, there is an
implementation of the security on the system that constrains how
to implement the programming model.

The framework manages applications and hides their security details.
To achieves it, the framework is built on top of security frameworks
of Linux: standard DAC and a MAC module SELinux or Smack.

## The programming model

The framework ensures that sensitive services, devices or resources
of the platform are protected. Applications can access these sensitive
resources only if explicitly permitted to do so.

Applications are packaged and delivered in a digitally signed RPM that
contains:

* the application and its data
* a configuration file `manifest.yml` in a directory `.rpconfig`
* some signature files in directory `.rpconfig`

```
IN SOME FUTURE

Signature make possible to allow or deny permissions required by the
application based on certificates of signers.

A chain of trust in the creation of certificates allows a hierarchical
structuring of permissions.
```

It also adds the description of dependency to other service because
redpesk programming model emphasis micro-services architecture design.

As today this model allows the distribution of HTML and binary applications
but it could be extended to any other class of applications.

## The security model

The security model refers to how DAC (Discretionary Access Control),
MAC (Mandatory Access Control) and Capabilities are used by the system
to ensure security and privacy.
It also includes features of reporting using audit features and by managing
logs and alerts.

The application framework uses the security model/framework
to ensure the security and the privacy of the applications that
it manages.

The security framework includes:

- **sec-lsm-manager**: component that interact with the security module of linux (Smack)
- **sec-cynagora**: component to manage permissions
- **afmpkg-daemon**: component to install and remove packages of the framework
- **redpesk**: RPM plugin in that interacts with afmpkg-installer
- **D-Bus** compliant to Cynagora: checks the permissions to deliver messages

**In theory, the security framework/model is an implementation details
that should not impact the programming model from a user point of view**.
