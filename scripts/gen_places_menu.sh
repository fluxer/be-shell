#!/bin/sh
SHELL_MENU="`kde4-config --path data`"
SHELL_MENU=${SHELL_MENU%%:*}be.shell/MainMenu/Places.xml
STREAM='<submenu><menu label="Places">'
STREAM="${STREAM} `xmlstarlet sel -T -t -m "/xbel/bookmark[info/metadata/IsHidden='false']" -o '<action label="' -v title -o '" exec="' -c "string(@href)" -o '"/>' -n $HOME/.local/share/user-places.xbel`"
STREAM="${STREAM} </menu></submenu>"
echo $STREAM > $SHELL_MENU