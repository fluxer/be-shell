#!/bin/sh
if (( ${#} < 3 )); then
    echo "Usage: $0 <base.jpg> <tile.png> [<center.png>] <result.bwp>"
    exit 1
fi

if [ ! -e "$1" ]; then
    echo "Base image \"$1\" does not exist"
    exit 1
fi
if [ ! -e "$2" ]; then
    echo "Tile image \"$2\" does not exist"
    exit 1
fi

if (( ${#} < 4 )); then
    TDIR="/tmp/`basename "${3}"`"
    TFILE="`basename "${3}"`"
else
    TDIR="/tmp/`basename "${4}"`"
    TFILE="`basename "${4}"`"
fi
mkdir -p "$TDIR"

cp "$1" "$TDIR/base.jpg"
cp "$2" "$TDIR/tile.png"
if (( ${#} > 3 )); then
    cp "$3" "$TDIR/center.png"
fi

pushd "$TDIR"  > /dev/null
TFILE="${TFILE%.bwp}.bwp"
if [ -e center.png ]; then
    tar -cJf "$TFILE" base.jpg tile.png center.png
else
    tar -cJf "$TFILE" base.jpg tile.png
fi
popd > /dev/null
mv "${TDIR}/${TFILE}" "${TFILE}"
rm -r "${TDIR}"