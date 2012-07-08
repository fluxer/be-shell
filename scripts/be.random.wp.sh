#!/bin/bash

# picks a random file out of a given dir (or subdir) and sets it as BE::Shell wallpaper

# Copyright (C) 2009-11 Thomas Luebking <thomas.luebking@web.de>
#
# This script is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public
# License version 2 as published by the Free Software Foundation.
#
# This script is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Library General Public License for more details.
#
# You should have received a copy of the GNU Library General Public License
# along with this library; see the file COPYING.LIB.  If not, write to
# the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
# Boston, MA 02110-1301, USA.


# CONFIG ---------------------------------

SRC_DIR="$HOME/.kde/share/wallpapers" # the directory in quest
DELAY=60 # the delay between daemon changes wallpaper in seconds
LOCK_FILE="/tmp/`whoami`/be.shell/random-wp.lock" # lockfile, stores the pid

set_wp()
{
    qdbus org.kde.be.shell /Desktop setWallpaper "$1" 0
}

# SCRIPT ------------------------------------

one_of()
{
    cnt=$(ls "$1" | wc -l)
    idx=$((RANDOM % cnt))
    candidate="$1/$(ls "$SRC_DIR" | head -n $idx | tail -n 1)"
    if [ -d "$candidate" ]; then
        echo -n "$(one_of "$candidate")"
    else
        echo -n "$candidate"
    fi
}


# DAEMONIZE ---------------------------------

case "$1" in
start)
    if [ ! -d "$SRC_DIR" ]; then
        echo "The wallpaper directoy $SRC_DIR does not exist"
        exit 1
    fi
    if [ ! -e "$LOCK_FILE" ]; then
        nohup $0 "daemon" >/dev/null 2>&1  &
        echo $! > "$LOCK_FILE"
    fi
    exit
    ;;
stop)
    if [ -e "$LOCK_FILE" ]; then
        pid=$(cat "$LOCK_FILE")
        kill $pid
        pkill -P $pid
    fi
    exit
    ;;
once)
    if [ ! -d "$SRC_DIR" ]; then
        echo "The wallpaper directoy $SRC_DIR does not exist"
        exit 1
    fi
    $(set_wp "$(one_of "$SRC_DIR")")  > /dev/null 2>&1
    exit
    ;;
daemon)
    ;;
*)
    echo -e "\nUsage:\n------\n   $0 start|stop|once\n"
    exit
esac

# ----------------------

trap 'rm "$LOCK_FILE"; exit' INT TERM EXIT

# MAIN LOOP ---------------------------------

while true; do
    $(set_wp "$(one_of "$SRC_DIR")")  > /dev/null 2>&1
    sleep $DELAY
done

rm $LOCK_FILE
trap - INT TERM EXIT



