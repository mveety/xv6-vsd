#!/bin/sh

sed -e 's/\"/\</' < $1 > $1.tmp
sed -t 's/\"/\>/' < $1.tmp > $1.conv
rm $1.tmp

