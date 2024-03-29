HISTORY OF THE AGL FRAMEWORK
============================

## Brief history

- March 2016: proposal of IoT.BzH based on refit of tizen framework
- December 2016: adoption for AGL
- March 2017: switch to systemd launcher
- November 2017: switch to systemd "system"
- May 2018: switch to meta-security
- June 2019: switch to multi-user (to agl-driver only)
- October 2019: switch to cynagora
- January 2020: Integrates Token logic compatible with OAuth2

## Origin of the AGL Application Framework

The Application Framework of AGL implements the security model
of Tizen 3 but differs from the Application Framework of Tizen
3.

During the first works in having the security model of Tizen
integrated in AGL (Automotive Grade Linux) distribution, it became
quickly obvious that the count of components specific to Tizen
to integrate was huge.

Here is a minimal list of what was needed:

- platform/appfw/app-installers
- platform/core/security/cert-svc
- platform/core/appfw/ail
- platform/core/appfw/aul-1
- platform/core/appfw/libslp-db-util
- platform/core/appfw/pkgmgr-info
- platform/core/appfw/slp-pkgmgr

But this list isn't complete because many dependencies are hidden.
Those hidden dependencies are including some common libraries but also many
tizen specific sub-components:

- iniparser
- bundle
- dlog,
- libtzplatform-config
- db-util
- vconf-buxton
- ...

This is an issue because AGL is not expected to be Tizen.
Taking it would either need to patch it for removing unwanted components
or to take all of them.

However, a careful study of the core components of the security framework
of Tizen showed that their dependencies to Tizen are light (and since some
of our work, there is no more dependency to tizen).

These components are :

- **cynara**
- **security-manager**
- **D-Bus aware of cynara**

Luckily, at the time of that work, 2016, these core security components of
Tizen were provided by [meta-intel-iot-security][meta-intel], a set of yocto
layers.

These layers were created by Intel to isolate Tizen specific security
components from the initial port of Tizen to Yocto.
The 3 layers are providing components for:

- Implementing Smack LSM
- Implementing Integrity Measurement Architecture
- Implementing Tizen Security Framework

We took the decision to use these security layers that provide the
basis of the Tizen security, the security framework.

For the components of the application framework, built top of
the security framework, instead of pulling the huge set of packages
from Tizen, we decided to refit it by developing a tiny set of
components that would implement the same behaviour but without all
the dependencies and with minor architectural improvements for AGL.

These components were :

- **afm-system-daemon**
- **afm-user-daemon**

They provides infrastructure for installing, uninstalling,
launching, terminating, pausing and resuming applications in
a multi user secure environment.

A third component exists in the framework, the binder **afb-daemon**.
The binder provides the easiest way to provide secured API for
any tier.

<!-- pagebreak -->

## Evolution of the application framework within AGL

After its first version in March 2016 and its adoption by AGL
in december 2016, the framework evolved slowly.

In March 2017, the application launcher of the framework was
fully replaced by systemd as a launcher. Many good reasons
advocated this replacement:

- one less component to maintain
- immediate availability of many advanced features (cgroups, security,
  automatic start, ...)
- systemd is now a well known and used standard
- the process of generating systemd services is tunable by implementors

In November 2017, after feedback from experiments with systemd and for
solving issues related to security of AGL, the user applications and services
are switched from the systemd user space to the system space using
parametric UID.

After Intel abandonned their [meta-intel-iot-security][meta-intel],
AGL switched to [meta-security][meta-security].

In June 2019, the started user is no more root and most of services
are running as not root and with lowered capabilities.

## Links between the "Security framework" and the "Application framework"

The security framework refers to the security model used to ensure
security and to the tools that are provided for implementing that model.

The security model refers to how DAC (Discretionary Access Control),
MAC (Mandatory Access Control) and Capabilities are used by the system
to ensure security and privacy.
It also includes features of reporting using audit features and by managing
logs and alerts.

The application framework manages the applications:

- installing
- uninstalling
- starting
- pausing
- listing
- ...

The application framework uses the security model/framework
to ensure the security and the privacy of the applications that
it manages.

The application framework must be compliant with the underlying
security model/framework.
But it should hide it to the applications.

## The security framework

The implemented security model is the security model of Tizen 3.
This model is described [here][tizen-secu-3].

The security framework then comes from Tizen 3 but through
the [meta-intel].
It includes:

- **Security-Manager**
- **Cynara**
- **D-Bus** compliant to Cynara.

Two patches are applied to the security-manager.
The goal of these patches is to remove specific dependencies with Tizen packages that are not needed by AGL.
None of these patches adds or removes any behaviour.

**In theory, the security framework/model is an implementation details
that should not impact the layers above the application framework**.

The security framework of Tizen provides "nice lad" a valuable component to
scan log files and analyse auditing.
This component is still in development.

## The application framework

The application framework on top of the security framework
provides the components to install and uninstall applications
and to run it in a secured environment.

The goal is to manage applications and to hide the details of
the security framework to the applications.

For the reasons explained in introduction, we did not used the
application framework of Tizen as is but used an adaptation of it.

The basis is kept identical:

- The applications are distributed in a digitally signed container that must
  match the specifications of widgets (web applications).

This is described by the technical recommendations [widgets] and
[widgets-digsig] of the W3 consortium.

This model allows:

- The distribution of HTML, QML and binary applications.
- The management of signatures of the widget packages.

This basis is not meant as being rigid and it can be extended in the
future to include for example incremental delivery.

[meta-intel]:       https://github.com/01org/meta-intel-iot-security                "A collection of layers providing security technologies"
[meta-security]:    https://git.yoctoproject.org/cgit/cgit.cgi/meta-security/       "security and hardening tools and libraries for Linux"
[widgets]:          http://www.w3.org/TR/widgets                                    "Packaged Web Apps"
[widgets-digsig]:   http://www.w3.org/TR/widgets-digsig                             "XML Digital Signatures for Widgets"
[libxml2]:          http://xmlsoft.org/html/index.html                              "libxml2"
[openssl]:          https://www.openssl.org                                         "OpenSSL"
[xmlsec]:           https://www.aleksey.com/xmlsec                                  "XMLSec"
[json-c]:           https://github.com/json-c/json-c                                "JSON-c"
[d-bus]:            http://www.freedesktop.org/wiki/Software/dbus                   "D-Bus"
[libzip]:           http://www.nih.at/libzip                                        "libzip"
[cmake]:            https://cmake.org                                               "CMake"
[security-manager]: https://wiki.tizen.org/wiki/Security/Tizen_3.X_Security_Manager "Security-Manager"
[app-manifest]:     http://www.w3.org/TR/appmanifest                                "Web App Manifest"
[tizen-security]:   https://wiki.tizen.org/wiki/Security                            "Tizen security home page"
[tizen-secu-3]:     https://wiki.tizen.org/wiki/Security/Tizen_3.X_Overview         "Tizen 3 security overview"
[AppFW-APP_install_sequences]: pictures/AppFW-APP_install_sequences.svg
[Security_model_history]: pictures/Security_model_history.svg
