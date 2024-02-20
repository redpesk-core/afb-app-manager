
# Known content types

The configuration file ***/etc/afm/afm-unit.conf*** defines
how to create systemd units for packages.

Known types for the type of content are:

- ***text/html***:
  HTML application, start a graphical client,
  content.src designates the home page of the application
  **(don't use it)**

- ***application/vnd.agl.native***
  AGL compatible native,
  content.src designates the relative path of the binary.

- ***application/vnd.agl.service***:
  AGL service, content.src is not used but must exist.

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

