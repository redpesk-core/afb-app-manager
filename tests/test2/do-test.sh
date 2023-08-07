#!/bin/bash

#DBG="gdb -arg "

mk=$(dirname $0)/mk-req.sh
tmpf=/tmp/request.txt

$mk ADD test2-main > $tmpf
$DBG ../../build/src/main/afm-check-pkg -vvvv $tmpf
$mk ADD test2-exp1 > $tmpf
$DBG ../../build/src/main/afm-check-pkg -vvvv $tmpf
$mk ADD test2-exp2 > $tmpf
$DBG ../../build/src/main/afm-check-pkg -vvvv $tmpf

$mk REMOVE test2-exp2 > $tmpf
$DBG ../../build/src/main/afm-check-pkg -vvvv $tmpf
$mk REMOVE test2-exp1 > $tmpf
$DBG ../../build/src/main/afm-check-pkg -vvvv $tmpf
$mk REMOVE test2-main > $tmpf
$DBG ../../build/src/main/afm-check-pkg -vvvv $tmpf
