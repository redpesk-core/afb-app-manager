Application framework
=====================

Foreword
--------

This document describes application framework fundamentals.
FCS (Fully Conform to Specification) implementation is still under development.
It may happen that current implementation somehow diverges with specifications.

Overview
--------

The application framework on top of the security framework
provides components to install and uninstall applications
as well as to run them in a secured environment.

The goal of the framework is to manage applications and hide security details
to applications.

For the reasons explained in introduction, it was choose not to reuse Tizen
application framework directly, but to rework a new framework inspired from Tizen.

fundamentals remain identical: the applications are distributed
in a digitally signed container that should match widget specifications
normalized by the W3C. This is described by the technical
recommendations [Packaged Web Apps (Widgets)](http://www.w3.org/TR/widgets) and [XML Digital Signatures for Widgets](http://www.w3.org/TR/widgets-digsig) of the W3 consortium.

As today this model allows the distribution of HTML, QML and binary applications
but it could be extended to any other class of applications.

The management of widget package signatures.
Current model is only an initial step, it might be extended in the
future to include new feature (ie: incremental delivery).
