#!/bin/sh
#
# Copying and distribution of this file, with or without modification,
# are permitted in any medium without royalty provided the copyright
# notice and this notice are preserved.  This file is offered as-is,
# without any warranty.

ORG="/C=FR/ST=Brittany/L=Lorient/O=IoT.bzh"

cat > extensions << EOC
[root]
basicConstraints=CA:TRUE
keyUsage=keyCertSign
subjectKeyIdentifier=hash
authorityKeyIdentifier=keyid
[derivate]
basicConstraints=CA:TRUE
keyUsage=keyCertSign,digitalSignature
subjectKeyIdentifier=hash
authorityKeyIdentifier=keyid
EOC

keyof() { echo -n "$1.key.pem"; }
certof() { echo -n "$1.cert.pem"; }

generate() {

local s="$1" n="$2" cn="$3" sig="$4"
local key="$(keyof "$n")" cert="$(certof "$n")"

if [ ! -f "$key" ]
then
	echo
	echo "generation of the $n key"
	openssl ecparam \
		-name secp256k1 -genkey \
		-outform PEM \
		-out "$key"
fi

if [ ! -f "$cert" -o "$key" -nt "$cert" ]
then
	echo
	echo "generation of the $n certificate"
	openssl req -new \
			-key "$key" \
			-subj "$ORG/CN=$cn" |
	openssl x509 -req \
			-days 3653 \
			-sha256 \
			-extfile extensions \
			-trustout \
			$sig \
			-set_serial $s \
			-setalias "$cn" \
			-out "$cert"
fi

}

genroot() {
	local s="$1" n="$2" cn="$3"
	generate "$s" "$n" "$cn" "-signkey $(keyof "$n") -extensions root"
}

derivate() {
	local s="$1" n="$2" cn="$3" i="$4"
	generate "$s" "$n" "$cn" "-CA $(certof "$i") -CAkey $(keyof "$i") -extensions derivate"
}


genroot 1 root "Root certificate"
derivate 2 developer "Root developer" root
derivate 3 platform "Root platform" root
derivate 4 partner "Root partner" root
derivate 5 public "Root public" root

rm extensions
