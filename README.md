Luvi: A distributed parallel video transcoder
=============================================

This program opens your input movie, streams the frames out to 
multiple computers (slaves), transcodes them in parallel, 
returns the transcoded frames back to the master computer and 
saves the resulting video to a new file.

Originally, the intent of this program was to take my MythTV 
recordings and use the computers in my house in parallel to convert 
them them to a high-quality Bluray-compatible format in a much 
faster time than a single computer could do.

DEPENDENCIES:
=============================================

A) ffmpeg 1.0 and higher: Compile or install them however you see fit

B) x264: The only codec supported right now for transcoding. With additional help, this can easily be improved.

C) Protocol Buffers: google's data exchange format for network communication

D) libxdo2: A nice library used to keep the computers from going to sleep while they video is being farmed out for transcoding.


USAGE:
=============================================
Usage is fairly manual right now.... a GUI would be a logical next step.

1. Run the master like this:
   
     $ ./master [ input video filename ] [ output.m2ts ] [ port # to which slaves should connect ] [ 0 | 1 ]
    
The last parameter:
			'1' means: transcode in parallel using slaves
			'0' means: transcode locally (for debugging purposes)
	Example:
	   
     $ ./master input.mpg output.m2ts 1264 1 # start and wait for slaves

At this point, the master will print:

    Ready for slaves.
    
    Waiting for slaves...
    
    Waiting for slaves...
    
    Waiting for slaves...

2. Then, on as many computers as you have, run the slaves, like this:

    $ ./slave [ address/name of master ] [ port # ]

	Example:
	  
    $ ./slave localhost 1264 # connect to master and start working

3. If you just want to make sure the encoding *works* first locally
   without any parallelism (for example to make sure you have the right
   ffmpeg libraries installed):

	Example:
  
    $ ./master input.mpg output.m2ts 1264 0  # will transcode immediately without slaves

CONFIGURING:
=============================================

Things like x264 bitrates, profiles, presets, GOP sizes, and so forth
are all hard-coded inside "common.c".

Eventually a configuration system will get written when I have free time.

Feel free to modify (common.c) the file until that time comes....

Patches welcome =)

HOW IT WORKS:
======================================

Parallelism works by grouping multiple GOPs together (currently almost 1000 frames,
spanning about 45-60 frames/GOP on average from the videos I've tested with) 
into "ranges" of GOPs.

Each "range" of GOPs is assigned to a slave. The range includes a "scene-change" GOP from
the previous range which ensures that encoding is done cleanly between the slaves. This
is done by retaining the last GOP of a range and sending it along to the next slave when
the next slave is chosen.

Everything happens in a single pass: All the video is transcoded in the same order
that it was read from the original video and written back out to the new video in that same
order. This is done by ordering the transcoded ranges into a FIFO while they are multiplexed
across all the slaves.

The audio is just copied, right now. It is not transcoded.

The resulting video is written to an MPEG Transport Stream container.

BUGS:
======================================
No bugs with the video transcoding or synchronization.

The audio copying is weird: It plays back fine in VLC / Mplayer on my desktop,
but unless I to do the following to get the audio stream to function
properly on a Bluray Disk (tsMuxeR won't accept it):

By doing the following:

    $ ffmpeg -i output.m2ts -vcodec copy -acodec copy -f mpegts -copyts result.m2ts

This some how "sanitizes" the copied audio stream in a way that I don't 
understand, but makes the audio stream compatible for Bluray. Perhaps someone 
could help me track down the problem in my code when I copy the audio packets.

PATCHES / Reporting Bugs:
======================================
If you find a bug *cough* segfault *cough*,
please for god's sake, run GDB first.

Patches welcome!
