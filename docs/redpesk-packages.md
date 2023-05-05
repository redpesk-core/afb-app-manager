# redpesk packages

What is called a *redpesk package** is simply a RPM package embedding
metadata for *redpesk application framework*.

The first format of the metadata, a XML file, was conforming
to the, now obsolete, [W3C standard for widgets](https://www.w3.org/TR/widgets/).
 from Tizen's format

For the sake of lisibility, evolution and adaptation to usages, redpesk
now understand a new metadata format called the manifest format, a YAML file.

That new manifest format is mainly made of the file `manifest.yml` that
must be in a subdirectory `.rpconfig`.

The first format is still in use but for new designs we recommend to
adopt the new manifest format.

The metadata of packages are carrying the below data:

- the name of the package

- the required permission for the package

- the required files' properties

- for each installed target:

  * its name
  * its description
  * its type
  * what service it requires
  * what service it provides
  * what specific permission it requires

![packaging](pictures/package.svg){:width="60%"}

It is also possible to add signature in packages.
This feature is still in development.

![packaging](pictures/signing.svg){:width="60%"}
