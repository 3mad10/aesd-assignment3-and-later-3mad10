#!/bin/sh

NUMARGS=$#

if [ $NUMARGS -ne 2 ]
then
   echo "Missing required argument(s) Input format : writer.sh <path/to/file> <writestr> "
   exit 1
else
    DIRNAME=$(dirname $1)
    FILENAME=$(basename $1)
    mkdir -p "$DIRNAME" && touch "$DIRNAME/$FILENAME"
    echo $2>$1
fi
