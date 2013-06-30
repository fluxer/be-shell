#!/bin/sh


FIFO=resources

TOPLINE="`top -b 2>/dev/null | head -n7 | tail -n1`"
CMD_OFF=`echo "${TOPLINE%COMMAND*}" | wc -c`
CPU_OFF=`echo "${TOPLINE% \%CPU*}" | wc -c`

daemon_func() {
    top -bd5 | cut -c1-11,${CPU_OFF}-$((CPU_OFF+10)),${CMD_OFF}- | grep --line-buffered --no-group-separator -A4 '%MEM' > /tmp/`whoami`/be.shell/resources
}

source `dirname "$0"`/be.fifo.daemon
