#!/bin/sh

candidates='llvm-config llvm-config-3.7'

for cand in $candidates; do
    if which $cand > /dev/null ; then
        echo $cand
        break
    fi
done
