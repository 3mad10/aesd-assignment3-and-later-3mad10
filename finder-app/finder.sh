#!/bin/sh

NUMARGS=$#

if [ $NUMARGS -ne 2 ]
then
   echo "Missing required argument(s) Input format : finder.sh <path/to/dir/to/search> <searchstr> "
   exit 1
else
   if [ -d $1 ]
   then
    NUMMATCHES=$(grep -o "$2" "$1"/* | wc -l)
    NUMFILES=$(ls $1 | wc -l)
    echo "The number of files are ${NUMFILES} and the number of matching lines are ${NUMMATCHES}"
   else
    echo "$1 Does not exist or is not a directory"
    exit 1
    fi
fi
