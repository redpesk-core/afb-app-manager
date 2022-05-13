# Document for memo, to be reordered

## signing the content and signed content

The tool **afm-signed-digest** allows to:

- make a signature (author or distributor)
- check a signature
- check all the signature and report allowed domains

The tool **afm-check-signature** reads a signature
file and if validated by a trusted certificates,
prints the allowed domains.

## computing digest

The program **afm-digest** is similar to the program
**shasum**. It is used to create the digest that is signed
by other processes.

## ANNEX

The content must be signed using a PKCS7 file.

Given the list of content in a file: CONTENT.LIST

The following command produces the finger print hash
of that content: CONTENT.HASH

  LANG= sort | xargs -n1 sha256sum --tag

The content hash can be checked using the command:

  sha256sum --check --status CONTENT.HASH

An authority can sign the hash using the PKCS.7
tools provoded by gnutls or openssl.

  certtool --infile CONTENT.HASH \
           --outfile CONTENT.SIGN.p7 \
           --p7-sign \
	   --p7-time \
	   --load-certificate certs/end.pem \
	   --load-privkey keys/end.key.pem

The signed content can be shown as below

 |certtool --p7-info --p7-show-data --infile CONTENT.SIGN.p7

It can be verified as below

  certtool --p7-info --p7-show-data --infile CONTENT.SIGN.p7 |
  sha256sum --check --status

Coming from the widget digsig specifications, the mechanism to sign
the content is performed by the author and optionnaly by distributors.
The content of the signatures differs:
 - the author signs the content files
 - distributors sign the content files and the author signature

Name of author signature: author-signature.p7

Name of distributor signatures: signatureN.p7 (N is a number not led with zero)

The above method has the drawback that it does not allow to package
neither empty directories nor symbolic links.

The previous way of managing file properties (execution flag) was to let
the installer putting it accordingly to the configuration.
The new behaviour must be checked for seeing if some attributes have to
be signed.

Also, accordingly to RPM framework, the list of installed file
could be deduced from RPM itself.

To address the specific issues, the simplest way is to allow addition
of files describing extra features. This files are then signed.

Author signature:

  cat CONTENT.HASH |
  certtool --p7-sign \
	   --p7-time \
	   --p7-include-cert \
	   --load-certificate AUTHOR-CERTIFICATE.pem \
	   --load-privkey AUTHOR-PRIVATE-KEY.pem |
  cat > author-signature.p7

Author check:

  cat author-signature.p7 |
  certtool --p7-info --p7-show-data |
  cmp - CONTENT.HASH &&
  sha256sum --check --status CONTENT.HASH

Distributor N signature:

  sha256sum CONTENT.HASH author-signature.p7 |
  tee DISTRIBUTOR.HASH |
  certtool --p7-sign \
	   --p7-time \
	   --p7-include-cert \
	   --load-certificate DISTRIBUTOR-CERTIFICATE.pem \
	   --load-privkey DISTRIBUTOR-PRIVATE-KEY.pem |
  cat > signatureN.p7

Distributor N check:

  sha256sum CONTENT.HASH author-signature.p7 |
  cat > DISTRIBUTOR.HASH
  cat signatureN.p7 |
  certtool --p7-info --p7-show-data |
  cmp - DISTRIBUTOR.HASH &&
  sha256sum --check --status DISTRIBUTOR.HASH &&
  sha256sum --check --status CONTENT.HASH


Before checking the content, the signatures have to be
validated against a chain of trust. To achieve this
the following command have to be used:

  cat SIGNATURE.p7 |
  certtool --p7-verify --load-ca-certificate ROOT-CERTIFICATE.pem

Or merely, if the trusted root is installed in the
platform.

  certtool --p7-verify < SIGNATURE.p7

The issue here is that the chain of trust must be fully
available to be validated. The simplest way of doing it
is to include in the PEM certificate used for signing
that chain of trust. Otherwise, the intermediate CA
have to be provisioned.

Once chain of trust and content validated, the question
of knowing what kind of permission is granted must be answered.

possible options:
 - don't include signed content in the PKCS7 file
 - include signed content in the file

