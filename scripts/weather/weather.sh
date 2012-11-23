#!/bin/bash

EXEC="`readlink -f $0`"
SPATH="`dirname $EXEC`"

# get translated text
if [ ! -z "$2" ]; then
    . "$SPATH/$2"
fi

DATA="`wget -qO- "http://weather.yahooapis.com/forecastrss?w=$1&u=c" | grep -E 'yweather:condition|yweather:forecast'`"

NEED_HR=false

if [ "$3" != "forecastonly" ]; then
    # resolves $text $code $temp and $date
    eval "`echo "$DATA" | sed -e '/yweather:condition/!d; s/^[^ ]*//g; s%\s*/>%%g; s/" /"; /g'`"

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
    REPLY="<div><img src=\"${ICON}\" width=\"48\"/><span style=\"font-size:32px;\">${temp}°</span></div><div align=\"right\" style=\"font-size:12px\">${TEXT}</div>"
    NEED_HR=true
fi

if [[ "$3" =~ "forecast" ]]; then

    # resolves $day $date $low $high $text and $code - hopefully
    eval "`echo "$DATA" | sed -e '/yweather:condition/d; s/^[^ ]*//g; s%\s*/>%%g; s/" /"; /g'`"

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
    if $NEED_HR; then
        REPLY="${REPLY}<hr>"
    fi
    REPLY="${REPLY}<div>`date -d "$date" +%A`</div><div><img src=\"${ICON}\" width=\"48\"/><span style=\"font-size:18px;\">${low}°-${high}°</span></div><div align=\"right\" style=\"font-size:12px\">${TEXT}</div>"
fi

echo "$REPLY"