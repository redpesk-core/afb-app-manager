The permissions
===============


Permission's names
------------------

The proposal here is to specify a naming scheme for permissions
that allows the system to be as stateless as possible. The current
current specification includes in the naming of permissions either
the name of the bound binding when existing and the level of the
permission itself. Doing this, there is no real need for the
framework to keep updated a database of installed permissions.

The permission names are [URN][URN] of the form:

  urn:AGL:permission:<api>:<level>:<hierarchical-name>

where "AGL" is the NID (the namespace identifier) dedicated to
AGL (note: a RFC should be produced to standardize this name space).

The permission names are made of NSS (the namespace specific string)
starting with "permission:" and followed by colon separated
fields. The 2 first fields are <api> and <level> and the remaining
fields are gouped to form the <hierarchical-name>.

	<api> ::= [ <pname> ]
	
	<pname> ::= 1*<pchars>
	
	<pchars> ::= <upper> | <lower> | <number> | <extra>
	
	<extra> ::= "-" | "." | "_" | "@"

The field <api> can be made of any valid character for NSS except
the characters colon and star (:*). This field designate the api
providing the permission. This scheme is used to deduce binding requirements
from permission requirements. The field <api> can be the empty
string when the permission is defined by the AGL system itself.
The field <api> if starting with the character "@" represents
a transversal permission not bound to any binding.

	<level> ::= 1*<lower>

The field <level> is made only of letters in lower case.
The field <level> can only take some predefined values:
"system", "platform", "partner", "tiers", "owner", "public".

	<hierarchical-name> ::= <pname> 0*(":" <pname>)

The field <hierarchical-name> is made <pname> separated by
colons. The names at left are hierarchically grouping the
names at right. This hierarchical behaviour is intended to
be used to request permissions using hierarchical grouping.

Permission's level
------------------


[URN]: https://tools.ietf.org/rfc/rfc2141.txt "RFC 2141: URN Syntax"

