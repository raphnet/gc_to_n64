#!/bin/bash

PREFIX=gc_to_n64
HEXFILE=$PREFIX.hex
VERSION=`grep VERSION= Makefile | cut -d '=' -f 2`

echo "Release script for $PREFIX"

if [ $# -ne 1 ]; then
	echo "Syntax: ./release.sh releasedir"
	echo
	echo "ex: './release releasedir' will produce $PREFIX-$VERSION.tar.gz in releasedir out of git HEAD."
	exit
fi

FULLNAME=$PREFIX-$VERSION.tar.gz

if [ -e $RELEASEDIR/$FULLNAME ]; then
	echo $FULLNAME already exists in $RELEASEDIR
	exit
fi

RELEASEDIR=`readlink -f $1`
DIRNAME=$PREFIX-$VERSION
FILENAME=$PREFIX-$VERSION.tar.gz
TAG=v$VERSION

echo "Version: $VERSION"
echo "Filename: $FILENAME"
echo "Release directory: $RELEASEDIR"
echo "--------"
echo "Ready? Press ENTER to go ahead (or CTRL+C to cancel)"

read

if [ -f $RELEASEDIR/$FILENAME ]; then
	echo "Release file already exists!"
	exit 1
fi

git tag $TAG -f -a -m "Release $VERSION"
git archive --format=tar --prefix=$DIRNAME/ HEAD | gzip > $RELEASEDIR/$FILENAME

cd $RELEASEDIR
echo "Untarring..."
tar zxf $FILENAME

echo "Building..."
cd $PREFIX-$VERSION
make clean
make

if [ ! -e gc_to_n64b.hex ]; then
	echo "Build error, missing artefact";
	exit
fi

cp -v gc_to_n64b.hex ../$PREFIX-$VERSION.hex

ls -lh ../$PREFIX-$VERSION.hex ../$PREFIX-$VERSION.tar.gz

