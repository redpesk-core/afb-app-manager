# Application framework of redpesk

## Overview

The application framework of **redpesk**
provides components to install and uninstall applications
as well as to run them in a secured environment.

The main functionalities are:

- install/uninstall applications and services

- start/terminate installed applications and services

- answer simple queries: what is installed? what runs?

The application framework fills the gap between the applications
development model and the effective system implementation.

In one hand, there is a programming model that includes security
features through permissions, micro service architecture through
flexible high level API, and, in the other hand, there is an
implementation of the security on the system that constrains how
to implement the programming model.

The framework manages applications and hides them security details.
To achieves it, the framework is built on top of a secured Linux.
The current implementation uses Smack and DAC Linux security modules (LSM).

The programming model and the security are inspired from Tizen 3.

## The programming model

The framework ensures that sensitive services, devices or resources
of the platform are protected. Applications can access these sensitive
resources only if explicitly permitted to do so.

Applications are packaged and delivered in a digitally signed container
named *widget*. A widget contains:

- the application and its data
- a configuration file *config.xml*
- signature files

The format of widgets is described by W3C (World Wide Web Consortium)
technical recommendations:

- [Packaged Web Apps (Widgets)](http://www.w3.org/TR/widgets)
  (note: now deprecated)

- [XML Digital Signatures for Widgets](http://www.w3.org/TR/widgets-digsig)

The format is enough flexible to include the description of permissions
and dependencies required or provided by the application.

Signature make possible to allow or deny permissions required by the
application based on certificates of signers.

A chain of trust in the creation of certificates allows a hierarchical
structuring of permissions.

It also adds the description of dependency to other service because AGL
programming model emphasis micro-services architecture design.

As today this model allows the distribution of HTML, QML and binary applications
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

The implemented security model comes from the security model of Tizen 3.
This model is described [here][tizen-secu-3].

The security framework includes:

- **sec-lsm-manager**: component that interact with the security module of linux (Smack)
- **sec-cynagora**: component to manage permissions
- **D-Bus** compliant to Cynagora: checks the permissions to deliver messages

**In theory, the security framework/model is an implementation details
that should not impact the programming model from a user point of view**.


[meta-intel]:       https://github.com/01org/meta-intel-iot-security                "A collection of layers providing security technologies"
[widgets]:          http://www.w3.org/TR/widgets                                    "Packaged Web Apps"
[widgets-digsig]:   http://www.w3.org/TR/widgets-digsig                             "XML Digital Signatures for Widgets"
[libxml2]:          http://xmlsoft.org/html/index.html                              "libxml2"
[openssl]:          https://www.openssl.org                                         "OpenSSL"
[xmlsec]:           https://www.aleksey.com/xmlsec                                  "XMLSec"
[json-c]:           https://github.com/json-c/json-c                                "JSON-c"
[d-bus]:            http://www.freedesktop.org/wiki/Software/dbus                   "D-Bus"
[libzip]:           http://www.nih.at/libzip                                        "libzip"
[cmake]:            https://cmake.org                                               "CMake"
[sec-lsm-manager]: https://wiki.tizen.org/wiki/Security/Tizen_3.X_Security_Manager  "Sec-lsm-Manager"
[app-manifest]:     http://www.w3.org/TR/appmanifest                                "Web App Manifest"
[tizen-security]:   https://wiki.tizen.org/wiki/Security                            "Tizen security home page"
[tizen-secu-3]:     https://wiki.tizen.org/wiki/Security/Tizen_3.X_Overview         "Tizen 3 security overview"
[AppFW-APP_install_sequences]: pictures/AppFW-APP_install_sequences.svg
[Security_model_history]: pictures/Security_model_history.svg
