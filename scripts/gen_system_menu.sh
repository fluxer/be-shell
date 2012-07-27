#!/bin/bash

APPDATA="`kde4-config --path data`"
APPDATA="${APPDATA%%:*}"
MENUFILE="$APPDATA/be.shell/MainMenu/settings.xml"
DIR="`kde4-config --path services`"
DIR="${DIR##*:}"

# scan for kcm modules and categories
CANDIDATES=`grep -Em1 "X-KDE-System-Settings-.*Category=[^ ]+" "$DIR"*.desktop | sed -e 's/=/\t/g' | sort -fk2 | cut -d':' -f1`

LAST_CATEGORY=""
GROUP=""
NO_CATEGORY=true
FIRST_ENTRY=""

XML="<submenu>\n\t<menu label=\"Settings\">"

for FILE in $CANDIDATES; do
    CATEGORY=""
    EXEC=""
    ICON=""
    NAME=""
    TYPE=""
    DATA=`grep -E '(X-KDE-System-Settings-Category=|X-KDE-System-Settings-Parent-Category=|X-KDE-ServiceTypes=|Exec=|Icon=|Name=)' "$FILE"`
    while read LINE; do
        case "${LINE%%=*}" in
        "Exec")
            EXEC="${LINE#*=}"
            ;;
        "Name")
            NAME="${LINE#*=}"
            ;;
        "Icon")
            ICON="${LINE#*=}"
            ;;
        "X-KDE-System-Settings-Category"|"X-KDE-System-Settings-Parent-Category")
            CATEGORY="${CATEGORY:=${LINE#*=}}"
            ;;
        "X-KDE-ServiceTypes")
            TYPE="${LINE#*=}"
            ;;
        *) # junk
        esac
    done < <(echo -e "$DATA\n")

    if [ "${CATEGORY}" != "${LAST_CATEGORY}" ] && [ ! -z "${LAST_CATEGORY}" ]; then
        if [ ! -z "$FIRST_ENTRY" ]; then # this is an empty group - happens
            if $NO_CATEGORY; then # some kcms don't have a "valid" parent, eg. adobe flash. name the group after the first entry
                GROUP="\t\t<menu label=\"${FIRST_ENTRY}\">${GROUP}"
            fi
            XML="${XML}\n${GROUP}\n\t\t</menu>"
        fi
        GROUP=""
        FIRST_ENTRY=""
        NO_CATEGORY=true
    fi

    if [ "$TYPE" = "SystemSettingsCategory" ]; then
        GROUP="\t\t<menu label=\"${NAME}\" icon=\"${ICON}\">${GROUP}"
        NO_CATEGORY=false
    else
        FIRST_ENTRY="${FIRST_ENTRY:=${NAME}}"
        GROUP="${GROUP}\n\t\t\t<action label=\"${NAME}\" exec=\"${EXEC}\"/>"
    fi

    LAST_CATEGORY="$CATEGORY"
done

XML="${XML}\n\t</menu>\n</submenu>"

echo -e $XML | sed -e 's/&\s/\&amp; /g' > "$MENUFILE"

touch "$APPDATA/be.shell/MainMenu.xml"