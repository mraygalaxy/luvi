#!/usr/bin/env bash

if [ $0 != "-bash" ] ; then
	pushd `dirname "$0"` 2>&1 > /dev/null
fi
dir=$(pwd)
if [ $0 != "-bash" ] ; then
	popd 2>&1 > /dev/null
fi

file=$1

if [ x"$file" == x ] ; then
    file="I_Love_Lucy-Ricky_Loses_His_Temper.mpg"
    log "copying $dir/../$file to /tmp/$file..."
    cp -f $dir/../$file /tmp/$file
    log "done"
    #echo "need filename"
    #exit 1
else
    rm -f /tmp/$file
fi

shift

src=$1

rm -f /tmp/source.mpg

if [ x"$src" == x ] ; then
    src="http://9.47.174.6:45005/hackathon/$file"
    #echo "need object store URI"
    #exit 1
fi

shift

rm -f /tmp/result.mpg

function log {
    echo "$1"
    #echo "$1" >> $dir/../luvi_master.log
}

orig=/tmp/$file

if [ ! -e $orig ] ; then
    log "Downloading from $src to $orig..."
    wget $src -O $orig 
    log "Done downloading"
else
    log "File is already here. Starting master"
fi

log "Starting master against: $orig"

/usr/bin/time /usr/local/bin/luvi_master $orig /tmp/result.mpg 4444 1 2>&1 >> $dir/../luvi_master.log

result=${file}_result
log "Uploading result to $result"

curl ${src}_result -X PUT -T /tmp/result.mpg -v

log "Done uploading"

rm -f /tmp/result.mpg

exit 0

