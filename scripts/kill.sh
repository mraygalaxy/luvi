#!/bin/bash

if [ $0 != "-bash" ] ; then
	pushd `dirname "$0"` 2>&1 > /dev/null
fi
dir=$(pwd)
if [ $0 != "-bash" ] ; then
	popd 2>&1 > /dev/null
fi

#chmod -R 777 /tmp/luvi-web
cd /tmp/luvi-web
pkill -9 -U nobody -f luvi_slave
pkill -9 -U nobody -f luvi_master

function log {
    echo "$1"
    echo "$1" >> $dir/../luvi_master.log
}

truncate -s 0 $dir/../luvi_master.log
log "Kill complete"

exit 0
