
# Known content types

The configuration file ***/etc/afm/afm-unit.conf*** defines
how to create systemd units for packages.

Known types for the type of content are:

- ***application/vnd.redpesk.native***
  repesk compatible native,
  content.src designates the relative path of the binary.

- ***application/vnd.redpesk.service***:
  repesk service, content.src is not used but must exist.

- ***application/x-executable***:
  Native application,
  content.src designates the relative path of the binary.
  For such application, only security setup is made.

- ***application/vnd.redpesk.resource***:
  Resource, not an application.
  content.src is not used but must exist.

- ***application/vnd.redpesk.httpd***:
  HTTP server only,
  content.src must exist but is not used

- ***application/vnd.agl.native***
  LEGACY, synonym of *application/vnd.redpesk.native*

- ***application/vnd.agl.service***:
  LEGACY, synonym of *application/vnd.redpesk.service*

