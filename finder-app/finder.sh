#!/bin/sh

filesdir=$1
searchstr=$2

if ! [ $# -eq 2 ]; then
	echo "Please provide correct number of arguments"
	exit 1
fi

if ! [ -d $filesdir ]; then
	echo "No such directory"
	exit 1
fi

x=`egrep -m 1 -R "$searchstr" $filesdir | wc -l`
y=`egrep -R "$searchstr" $filesdir | wc -l`

echo The number of files are $x and the number of matching lines are $y
