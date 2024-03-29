#!/bin/bash

EXEC="`readlink -f $0`"
SPATH="`dirname $EXEC`"

# calc sizes
ICON_SIZE=${2:-48}
if (( $ICON_SIZE > 15 )); then
    ICON_SIZE=$ICON_SIZE # catch nan parameters
else
    ICON_SIZE=48
fi
BIG_TEXT=$((ICON_SIZE*2/3))
MID_TEXT=$((ICON_SIZE*9/24))
SMALL_TEXT=$((ICON_SIZE/4))

# get translated text
if [ ! -z "$4" ]; then
    . "$SPATH/$4"
fi

l10n() {
    TEXT=${CONDITION[$1]}
    if [ -z "$TEXT" ]; then
        echo "$2"
    else
        echo "$TEXT"
    fi
}

icon() {
    ICON="${SPATH}/icons/$1.png"
    if (($1 < 10)); then
        if [ -e "${SPATH}/icons/0${1}.png" ]; then
            ICON="${SPATH}/icons/0${1}.png"
        fi
    fi
    echo $ICON
}

WAIT_FOR_NET=true
while $WAIT_FOR_NET; do
    if ping -c1 weather.yahooapis.com > /dev/null 2>&1; then
        WAIT_FOR_NET=false
    else
        sleep 30
    fi
done

DATA="`wget -qO- "http://weather.yahooapis.com/forecastrss?w=$1&u=c" | grep -E 'yweather:condition|yweather:forecast'`"

NEED_HR=false

# get translated text
MODE="$3"
if [ -z "$MODE" ]; then
    MODE="now"
fi

REPLY="<table><tr>"
if [ "$5" = "h" ]; then
    TEXT_ALIGN="center"
else
    TEXT_ALIGN="right"
fi

if [[ "$MODE" =~ "now" ]]; then
    # resolves $text $code $temp and $date
    eval "`echo "$DATA" | sed -e '/yweather:condition/!d; s/^[^ ]*//g; s%\s*/>%%g; s/" /"; /g'`"

    TEXT="$(l10n $code $text)"
    ICON="$(icon $code)"
    REPLY="${REPLY}<td>"
    if [ ! "$MODE" = "now" ]; then
        REPLY="${REPLY}<div>Now</div>"
    fi
    REPLY="${REPLY}<div><img src=\"${ICON}\" width=\"${ICON_SIZE}\"/><span style=\"font-size:${BIG_TEXT}px;\">${temp}°</span></div><div align=\"${TEXT_ALIGN}\" style=\"font-size:${SMALL_TEXT}px\">${TEXT}</div></td>"
    if [ "$5" = "v" ]; then
        REPLY="${REPLY}</tr><tr>"
        NEED_HR=true
    fi
fi

if [[ "$MODE" =~ "today" ]]; then
    # resolves $text $code $temp and $date
    eval "`echo "$DATA" | sed -e '0,/yweather:forecast/! d; s/^[^ ]*//g; s%\s*/>%%g; s/" /"; /g'`"

    TEXT="$(l10n $code $text)"
    ICON="$(icon $code)"

    REPLY="${REPLY}<td>"
    if $NEED_HR; then
        REPLY="${REPLY}<hr>"
    fi
    REPLY="${REPLY}<div>`date -d "$date" +%A`</div><div><img src=\"${ICON}\" width=\"${ICON_SIZE}\"/><span style=\"font-size:${MID_TEXT}px;\">${low}°-${high}°</span></div><div align=\"${TEXT_ALIGN}\" style=\"font-size:${SMALL_TEXT}px\">${TEXT}</div></td>"
    if [ "$5" = "v" ]; then
        REPLY="${REPLY}</tr><tr>"
        NEED_HR=true
    fi
fi

if [[ "$MODE" =~ "tomorrow" ]]; then
    # resolves $text $code $temp and $date
    eval "`echo "$DATA" | sed -e '/yweather:condition/d; 1,/yweather:forecast/!d; s/^[^ ]*//g; s%\s*/>%%g; s/" /"; /g'`"

    TEXT="$(l10n $code $text)"
    ICON="$(icon $code)"

    REPLY="${REPLY}<td>"
    if $NEED_HR; then
        REPLY="${REPLY}<hr>"
    fi
    REPLY="${REPLY}<div>`date -d "$date" +%A`</div><div><img src=\"${ICON}\" width=\"${ICON_SIZE}\"/><span style=\"font-size:${MID_TEXT}px;\">${low}°-${high}°</span></div><div align=\"${TEXT_ALIGN}\" style=\"font-size:${SMALL_TEXT}px\">${TEXT}</div></td>"
fi

if [[ "$MODE" =~ "week" ]]; then
    for ((i=3;i<8;++i)); do
        # resolves $text $code $temp and $date
        eval "`echo "$DATA" | sed -e '/yweather:forecast/!d;'$i',6d; s/^[^ ]*//g; s%\s*/>%%g; s/" /"; /g'`"

        TEXT="$(l10n $code $text)"
        ICON="$(icon $code)"

        REPLY="${REPLY}<td>"
        if $NEED_HR; then
            REPLY="${REPLY}<hr>"
        fi
        if [ "$5" = "v" ]; then
            REPLY="${REPLY}</tr><tr>"
            NEED_HR=true
        fi
        REPLY="${REPLY}<div>`date -d "$date" +%A`</div><div><img src=\"${ICON}\" width=\"${ICON_SIZE}\"/><span style=\"font-size:${MID_TEXT}px;\">${low}°-${high}°</span></div><div align=\"${TEXT_ALIGN}\" style=\"font-size:${SMALL_TEXT}px\">${TEXT}</div></td>"
    done
fi

REPLY="${REPLY}</tr></table>"

echo "$REPLY"
