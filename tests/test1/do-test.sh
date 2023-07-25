#!/bin/bash

#DBG="gdb -arg "
tmpf=/tmp/request.txt
sed "s;\<CWD\>;$PWD;" request-add.txt > $tmpf
$DBG ../../build/src/main/afm-check-pkg -vvvv $tmpf
sed "s;\<CWD\>;$PWD;" request-remove.txt > $tmpf
#$DBG ../../build/src/main/afm-check-pkg -vvvv $tmpf
rm $tmpf
