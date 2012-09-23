#!/bin/sh


FIFO=resources

daemon_func() {
    top -bd5 | cut -c1-11,42-51,62- | grep --line-buffered --no-group-separator -A4 '%MEM' > /tmp/`whoami`/be.shell/resources
}

source `dirname "$0"`/be.fifo.daemon
