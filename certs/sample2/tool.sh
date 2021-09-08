#!/bin/sh
#-----------------------------
BASE=$(dirname $0)
CERTS=$BASE/certs
KEYS=$BASE/keys
TEMPLS=$BASE/templates
#-----------------------------

#-----------------------------
make_root_certificate() {
	local name=${1:-root}
	certtool \
		--generate-privkey \
		--key-type=rsa-pss \
		--no-text \
		--outfile=$KEYS/$name.key.pem
	certtool \
		--generate-self-signed \
		--template=$TEMPLS/mkroot.cfg \
		--load-privkey=$KEYS/$name.key.pem \
		--no-text \
		--outfile=$CERTS/$name.pem
}
#-----------------------------
make_sub_certificate() {
	local name=${1:-sub} auth=${2:-root}
	certtool \
		--generate-privkey \
		--key-type=rsa-pss \
		--no-text \
		--outfile=$KEYS/$name.key.pem
	certtool \
		--generate-certificate \
		--template=$TEMPLS/mksub.cfg \
		--load-privkey=$KEYS/$name.key.pem \
		--load-ca-privkey=$KEYS/$auth.key.pem \
		--load-ca-certificate=$CERTS/$auth.pem \
		--no-text \
		--outfile=$CERTS/$name.pem
}
#-----------------------------
make_end_certificate() {
	local name=${1:-end} auth=${2:-sub}
	certtool \
		--generate-privkey \
		--key-type=rsa-pss \
		--no-text \
		--outfile=$KEYS/$name.key.pem
	certtool \
		--generate-certificate \
		--template=$TEMPLS/mkend.cfg \
		--load-privkey=$KEYS/$name.key.pem \
		--load-ca-privkey=$KEYS/$auth.key.pem \
		--load-ca-certificate=$CERTS/$auth.pem \
		--no-text \
		--outfile=$CERTS/$name.pem
}
#-----------------------------
ACTION=$1
shift
case $ACTION in
  mkroot) make_root_certificate "$@";;
  mksub) make_sub_certificate "$@";;
  mkend) make_end_certificate "$@";;
esac


