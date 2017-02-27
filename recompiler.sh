#!/bin/sh

if [ "$#" -ne 1 ] || [ "$1" -gt 1 ] || [ "$1" -lt 0 ]; then
    echo "Usage : ./recompiler.sh <0|1>, with 0 or 1 indicating if we compile as contact point or not."
else
    make veryclean
    if [ "$1" -eq 0 ]; then
	make
    else
	make WITH_CORE=1
    fi
fi

