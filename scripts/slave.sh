#!/usr/bin/env bash

if [ $0 != "-bash" ] ; then
	pushd `dirname "$0"` 2>&1 > /dev/null
fi
dir=$(pwd)
if [ $0 != "-bash" ] ; then
	popd 2>&1 > /dev/null
fi

master=$1

if [ x"$master" == x ] ; then
    master="9.47.174.84"
    #echo "need master IP address"
    #exit 1
fi

shift

/usr/local/bin/luvi_slave $master 4444 2>&1 >> $dir/../luvi_slave.log
