#!/bin/sh
# tst.sh

gcc -Wall -DSELFTEST -o rtlim rtlim.c
if [ $? -ne 0 ]; then exit 1; fi

./rtlim
