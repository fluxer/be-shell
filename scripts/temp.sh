#!/bin/sh
renice -n 20 $$ >/dev/null 2>/dev/null
sensors -A | sed -r '/fan1|fan4|temp1|temp3/ ! d; s/fan1:     /CPU: /; s/fan4:      /Board:/; s/temp1:     /CPU:  /; s/temp3:     /Board:/'
echo -n "GPU: "; nice -n 20 nvidia-settings -tq GPUCoreTemp  -tq GPUCurrentClockFreqs 2>/dev/null | sed -e ':a;N;$!ba;s/\([^\n]*\)[\n]*/\1° /'
