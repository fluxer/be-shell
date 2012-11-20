#!/bin/bash

EXEC="`realpath $0`"
SPATH="`dirname $EXEC`"

# resolves $text $code $temp and $date

eval "`wget -qO- "http://weather.yahooapis.com/forecastrss?w=$1&u=c" | sed -e '/yweather:condition/!d; s/^[^ ]*//g; s%\s*/>%%g; s/" /"; /g'`"

# get translated text
if [ ! -z "$2" ]; then
    . "$SPATH/$2"
fi
TEXT=${CONDITION[$code]}
if [ -z "$TEXT" ]; then
    TEXT="$text"
fi

# seek icon for code, in doubt prepend "0"
ICON="${SPATH}/icons/${code}.png"
if ((code < 10)); then
    if [ -e "${SPATH}/icons/0${code}.png" ]; then
        ICON="${SPATH}/icons/0${code}.png"
    fi
fi

# print html for label
echo "<div><img src=\"${ICON}\" width=\"48\"/><span style=\"font-size:32px;\">${temp}°</span></div><div align=\"right\" style=\"font-size:12px\">${TEXT}</div>"
