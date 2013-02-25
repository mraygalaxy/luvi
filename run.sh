#!/bin/bash -x

# Just a helper script for running stuff

#file="../orig.mpg"
#file="../st.m2ts"
#file="/storage/recordings_readable/bytitle/Star Trek- First Contact/20121004-2100-2330- Untitled.mpg"
file="/storage/recordings_readable/bytitle/Star Trek II- The Wrath of Khan/20121203-2230-0100- Untitled.mpg"
#file="/storage/recordings_readable/bytitle/Star Trek III- The Search for Spock/20121004-1100-1330- Untitled.mpg"
#file="sample.mpg"
#file="sample2.mpg"
#file="sample3.mpg"
#file="../sample.m2ts"

#dir=$(echo "$file" | grep -oE ".*\/")
dir=/home/mrhines/mythtv_to_dvd/
sudo chmod 777 "$dir"

#video="${dir}output.m2ts"
video="${dir}output.mkv"

echo "input: $file"
echo "output: $video"

if [ x"$1" != xslave ] ; then
	rm -f "$video"
fi

# Rebuild in case of any changes
make


if [ $? -eq 0 ] ; then 
	if [ x"$1" == xslave ] ; then
		rm -f luvi_slave_$(hostname).log
		#gdb ~/mythtv_to_dvd/luvi/luvi_slave -ex "run $2 1264"
		nice -n 19 ~/mythtv_to_dvd/luvi/luvi_slave $2 1264
	else
		rm -f luvi_master.log
		#gdb ~/mythtv_to_dvd/luvi/luvi_master -ex "run \"${file}\" \"$video\" 1264 1"
		#nice -n 19 ~/mythtv_to_dvd/luvi/luvi_master "${file}" "$video" 1264 0
		nice -n 19 ~/mythtv_to_dvd/luvi/luvi_master "${file}" "$video" 1264 1
	fi
fi
