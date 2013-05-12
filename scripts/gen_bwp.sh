#!/bin/sh
if (( ${#} < 3 )); then
    echo "Usage: $0 <base.jpg> <tile.png> <result.bwp>"
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
TDIR="/tmp/`basename "${3}"`"
mkdir -p "$TDIR"

cp "$1" "$TDIR/base.jpg"
cp "$2" "$TDIR/tile.png"

pushd "$TDIR"  > /dev/null
TFILE="`basename "${3}"`"
TFILE="${TFILE%.bwp}.bwp"
tar -cJf "$TFILE" base.jpg tile.png
popd > /dev/null
mv "${TDIR}/${TFILE}" "${TFILE}"
rm -r "${TDIR}"