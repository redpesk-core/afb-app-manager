#!/bin/bash

cd $(dirname $0)
rm -r tmp
mkdir -p tmp

cmd="afm-translate -t afm-unit.conf manifest.yml meta.yml -u ROOT"
$cmd -l > tmp/out.legacy
$cmd -m > tmp/out.modern
$cmd -s | tar -C tmp -x

diff -r ref tmp

