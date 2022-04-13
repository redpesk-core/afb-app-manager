
# Known content types

The configuration file ***/etc/afm/afm-unit.conf*** defines
how to create systemd units for packages.

Known types for the type of content are:

- ***text/html***:
  HTML application,
  content.src designates the home page of the application

- ***application/vnd.agl.native***
  AGL compatible native,
  content.src designates the relative path of the binary.

- ***application/vnd.agl.service***:
  AGL service, content.src is not used.

- ***application/x-executable***:
  Native application,
  content.src designates the relative path of the binary.
  For such application, only security setup is made.

Adding more types is easy, it just need to edit the configuration
file ***afm-unit.conf***.

