Permissions of applications are splitted in domains

Each domain can be granted or not depending on the authority
that signed the application.

An authority sign an application with a pkcs7 signature of the
content of the application. That signature is done using a certificate.
The certificate and its issuers are added to the signature.

This allow to check that certificates have or not the property
of granting a domain of permissions.

This can be done using the "KEY PURPOSE" (key_purpose_oid)
ability of of X509 certificates. Unfortunately, this ability
does not fit our purpose because all the chain has to have
the purpose, meaning that no distinction can be done between
authorities delivering certificates and authorities delivering
domains of permissions.

Permissions to domains are then given using an arbitrary extension
(add_extension) whose value is a codified string.

OID 1.3.9.812.383.370.00036.1

The widget certificate capability is a string indicating the what the certificate is able to do
when managing widgets permissions.

The string must match the regular expression (ERE):

([+#@][system,platform,partner,public](,[+*@][system,platform,partner,public])*)?

Where the "prefixing" character ([+#@]) means:

    +: signature grant permissions of the given domain
    #: can deliver certificate for granting permissions (having only + where current has #)
    @: can deliver any kind of certificate

Note that a certificate can not deliver a certificate for a permission and grant a permission. This is intended.
