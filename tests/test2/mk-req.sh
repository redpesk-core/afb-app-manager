#!/bin/bash

op=$1
dir=$2

cat << EOC
BEGIN $op
INDEX 1
COUNT 1
TRANSID $dir
PACKAGE $dir
ROOT /
$(find $dir|sed "s:^:FILE $PWD/:")
END $op
EOC
