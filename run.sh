#!/bin/bash -x

# Just a helper script for running stuff

#file="../st.mpg"
#file="../st.m2ts"
file="/storage/recordings_readable/bytitle/Star Trek- First Contact/20121004-2100-2330- Untitled.mpg"
#file="sample.mpg"
#file="sample2.mpg"
#file="sample3.mpg"

video=~/mythtv_to_dvd/luvi/output.m2ts

if [ x"$1" != xslave ] ; then
	rm -f "$video"
fi

# Rebuild incase of any changes
make

if [ $? -eq 0 ] ; then 
	if [ x"$1" == xslave ] ; then
		#gdb ~/mythtv_to_dvd/luvi/luvi_slave -ex "run $2 1264"
		nice -n 19 ~/mythtv_to_dvd/luvi/luvi_slave $2 1264
	else
		#gdb ~/mythtv_to_dvd/luvi/luvi_master -ex "run \"${file}\" \"$video\" 1264 1"
		#nice -n 19 ~/mythtv_to_dvd/luvi/luvi_master "${file}" "$video" 1264 0 # build index
		#nice -n 19 ~/mythtv_to_dvd/luvi/luvi_master "${file}" "$video" 1264 0 # local transcode
		nice -n 19 ~/mythtv_to_dvd/luvi/luvi_master "${file}" "$video" 1264 1 # slave transcode
	fi
fi
