#!/bin/sh
DATA_PATH="`kde4-config --path data`"
DATA_PATH="${DATA_PATH%%:*}"
STREAM="<submenu>"
for file in "${DATA_PATH}RecentDocuments/"*; do
    eval "`sed -r '/^(Icon|Name|URL|X-KDE-LastOpenedWith).*/!d; s/URL\[.[^\]]*/URL/g; s/X-KDE-LastOpenedWith/LastOpenedWith/g; s/=(.*)/="\1"/g' "$file"`"
    LastOpenedWith="${LastOpenedWith:-kfmclient open}"
    STREAM="${STREAM}<action label=\"${Name}\" icon=\"${Icon}\" exec=\"${LastOpenedWith} '${URL}'\" />\n"
#     echo $Icon $Name $URL
done
STREAM="${STREAM}</submenu>"

echo $STREAM > "${DATA_PATH}be.shell/MainMenu/RecentFiles.xml"