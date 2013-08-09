#!/bin/bash
# Better way of getting absolute path instead of relative path
if [ $0 != "-bash" ] ; then
	pushd `dirname "$0"` 2>&1 > /dev/null
fi
dir=$(pwd)
if [ $0 != "-bash" ] ; then
	popd 2>&1 > /dev/null
fi

for pid in $(pgrep -f luvisync) ; do
	if [ $pid != $$ ] && [ $pid != $PPID ] ; then
		kill -9 $pid
	fi
done

DESTINATIONS="localhost 9.47.174.84 9.47.174.85 9.47.174.86 9.47.174.106 9.47.174.107"

user="root"

for DEST in ${DESTINATIONS}; do
    ssh -t -t $user@$DEST "sudo chmod -R 777 /tmp/luvi-web"
    echo "$DEST" > $dir/debug.remote_host
    cp $dir/lsync.config.orig.root $dir/lsync.config.$DEST
    sed -ie "s/DESTINATION/$DEST/g" $dir/lsync.config.$DEST
    lsyncd -nodaemon -delay 0 -log Exec $dir/lsync.config.$DEST &
    ssh -t -t ubuntu@$DEST "sudo chmod -R 777 /tmp/luvi-web; sudo pkill -9 -U nobody -f luvi_relay; sudo su nobody -c 'screen -d -m -S luvirelay bash -c /tmp/luvi-web/luvi_relay.py 2>&1 > /tmp/luvi-web/luvi_relay.log'"
    disown
done
