#!/bin/sh

writefile=$1
writestr=$2

if ! [ $# -eq 2 ]; then
	echo "Please provide correct number of arguments"
	exit 1
fi

writedir=$(dirname "$writefile")

if ! [ -d $writedir ]; then
	echo "No such directory. Creating one!"
	mkdir -p $writedir
fi

echo $writestr > $writefile
