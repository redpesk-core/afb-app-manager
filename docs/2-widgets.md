# The widgets

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

Note that the technical recommendation
[Packaged Web Apps (Widgets)](http://www.w3.org/TR/widgets)
is now obsolete (since 11 october 2018).
It implies that a new format of widgets can be proposed in replacement.


The format is enough flexible to include the description of permissions
and dependencies required or provided by the application.

Signature make possible to allow or deny permissions required by the
application based on certificates of signers.

A chain of trust in the creation of certificates allows a hierarchical
structuring of permissions.

It also adds the description of dependency to other service because the
programming model emphasis micro-services architecture design.

