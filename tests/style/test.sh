#!/bin/bash

# Copyright 2015 Outscale SAS
#
# This file is part of Packetgraph.
#
# Packetgraph is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License version 3 as published
# by the Free Software Foundation.
#
# Packetgraph is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with Packetgraph.  If not, see <http://www.gnu.org/licenses/>.

usage="test.sh PACKETGRAPH_ROOT"

# test number of arguments

if [ "$#" -ne 1 ]; then
	echo echo $usage
	exit 1
fi

# check packetgraph root

PACKETGRAPH_ROOT=$(readlink -m $1)

if [ ! -d $PACKETGRAPH_ROOT ]; then
	echo "$PACKETGRAPH_ROOT path seems to not exists"
fi

# directories and files to scan
# Note: using -path ./folder_to_exclude -prune
c_filelist=$(find $PACKETGRAPH_ROOT/src/ -path $PACKETGRAPH_ROOT/src/npf -prune -o -type f -name "*.c" -printf %p\ )
h_filelist=$(find $PACKETGRAPH_ROOT/{src,include} -path $PACKETGRAPH_ROOT/src/npf -prune -o -type f -name "*.h" -printf %p\ )
# checkpatch tests

$PACKETGRAPH_ROOT/tests/style/checkpatch.pl --ignore TRAILING_SEMICOLON --show-types --no-tree -q -f $c_filelist $h_filelist
if [ $? != 0 ]; then
    echo "checkpatch tests failed"
    exit 1
else
    echo "checkpatch tests OK"
fi

# lizard tests

lizard &> /dev/null
if [ $? != 0 ]; then
    echo "lizard is not installed, some tests will be skipped"
else
    lizard -C 10 -w $directories
    if [ $? != 0 ]; then
	echo "lizard tests failed"
	exit 1
    else
        echo "lizard tests OK"
    fi
fi

# cppcheck tests

cppcheck -h &> /dev/null
if [ $? != 0 ]; then
    echo "cppcheck is not installed, some tests will be skipped"
else
    version=$(cppcheck --version| cut -f 2 -d ' ')
    major=$(echo $version| cut -f 1 -d '.')
    minor=$(echo $version| cut -f 2 -d '.')
    if [[ $major -lt 1 || $minor -lt 71 ]]; then
        echo "cppcheck version >= 1.71 required (currently have ${major}.${minor}), skip test"
    else
        cppcheck -q  -f -I $PACKETGRAPH_ROOT/src -I $PACKETGRAPH_ROOT/include --error-exitcode=43 --enable=style --enable=performance --enable=portability --enable=information --enable=missingInclude --enable=warning $c_filelist
        if [ $? != 0 ]; then
	    echo "cppcheck tests failed"
            exit 1
        else
            echo "cppcheck tests OK"
        fi
    fi
fi

# rats tests

rats &> /dev/null
if [ $? != 0 ]; then
    echo "rats is not installed, some tests will be skipped"
else
    rats  $c_filelist
    if [ $? != 0 ]; then
	echo "rats tests failed"
	exit 1
    else
        echo "rats tests OK"
    fi
fi

# flawfinder tests

flawfinder &> /dev/null
if [ $? != 0 ]; then
    echo "flawfinder is not installed, some tests will be skipped"
else
    flawfinder --quiet $c_filelist
    if [ $? != 0 ]; then
	echo "flawfinder tests failed"
	exit 1
    else
        echo "flawfinder tests OK"
    fi
fi

exit 0
