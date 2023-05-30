# this content is a research topic

## definition of domains

Permissions of applications are splitted in domains

Each domain can be granted or not depending on the authority
that signed the application.

An authority sign an application with a pkcs7 signature of the
content of the application. That signature is done using a certificate.
The certificate and its issuers are added to the signature.

This allow to check that certificates have or not the property
of granting a domain of permissions.

## details of the certificates

The check could have been done by using the "KEY PURPOSE" (key_purpose_oid)
ability of of X509 certificates. Unfortunately, this ability
does not fit our purpose because all the chain has to have
the purpose, meaning that no distinction can be done between
authorities delivering certificates and authorities delivering
domains of permissions.

Permissions to domains are then given using an arbitrary extension
(add_extension) whose value is a codified string.

OID 1.3.9.812.383.370.36.1

The certificate capability is a string that explain what
the certificate is able to do.

For the current version, the string is something like below:

```
+public,+partner,+platform,+system
```

This string tells that packages signed with such certificate
have granted accesses to the domains `public`, `partner`, `platform`
and `system`.

Formaly, the domains are listed and a character prefix explains
the purpose for the domain. The purposes are:

*  `+`: signature grant permissions of the given domain
*  `#`: can deliver certificate for granting permissions (having only + where current has #)
*  `@`: can deliver any kind of certificate


More precisely, the string must match the grammar below:

  SPECIFICATION ::= CAPABILITY
                  | SPECIFICATION ',' CAPABILITY

  CAPABILITY ::= PREFIX DOMAIN

  PREFIX ::= '+' | '#' | '@'

  DOMAIN ::= ANY CHARACTER EXCEPT ','

Note that a certificate can not deliver a certificate and grant the same domain.
This behaviour is intended.

## a tool for domain's specifications

The program **afm-domain-spec** can be used for checking
accesses.

Examples:

Displays the granted domains of a domain specification

```
> afm-domain-spec granted +toto,+titi
toto
titi
> afm-domain-spec granted +toto,#titi
toto
> 
```

Check if one or more domain is granted or not. The exit status
is 0 if granted or not zero when not granted.

```
> afm-domain-spec check-grant +toto,#titi
> echo $?
0
> afm-domain-spec check-grant +toto,#titi toto
> echo $?
0
> afm-domain-spec check-grant +toto,#titi toto titi
> echo $?
2
```

Check if a domain specification allows to emit certificate
with a the given specifications.

```
> afm-domain-spec check-cert +toto,#titi +toto
> echo $?
2
> afm-domain-spec check-cert +toto,#titi +titi
> echo $?
0
> afm-domain-spec check-cert +toto,#titi \#titi
> echo $?
2
> afm-domain-spec check-cert '#toto,@titi' '+toto,#titi'
> echo $?
0
```
