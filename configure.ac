# initial information about the project
AC_PREREQ([2.68])
AC_INIT([luvi],[1.0],[michael@hinespot.com])

# check if the source folder is available
AC_CONFIG_SRCDIR([common.c])
AC_CONFIG_HEADERS([config.h])

# automake initialization (mandatory) including a check for automake API version >= 1.9
AM_INIT_AUTOMAKE([1.9])
AC_PROG_CC
AC_PROG_MAKE_SET
AC_PROG_INSTALL

CFLAGS="$CFLAGS -I/usr/local/include"
LDFLAGS="$LDFLAGS -L/usr/local/lib"

AC_CHECK_PROG(PROTOBUF, protoc-c, protoc-c)

if test x"$PROTOBUF" != x"protoc-c" ; then
	AC_MSG_ERROR([Google protocol buffers compiler is missing: apt-get install protobuf-c-compiler libprotobuf-c0-dev])
fi

#AC_CHECK_LIB([protobuf-c], [protobuf_c_message_pack], [AC_MSG_RESULT([it works it works!!!!!!!!!!!!!])], [AC_MSG_ERROR([-lprotobuf-c missing: apt-get install protobuf-c-compiler libprotobuf-c0-dev])])
AC_CHECK_LIB([protobuf-c], [protobuf_c_message_pack],[], [AC_MSG_ERROR([-lprotobuf-c missing: apt-get install protobuf-c-compiler libprotobuf-c0-dev])])
AC_CHECK_LIB([xdo], [xdo_new], [], [AC_MSG_ERROR([-lxdo missing: apt-get install libxdo2 libxdo-dev])])
AC_CHECK_LIB([va], [vaCreateBuffer], [], [AC_MSG_ERROR([-lva missing: apt-get install libva1 libva-dev])])
AC_CHECK_LIB([bz2], [BZ2_bzDecompressInit], [], [AC_MSG_ERROR([-lbz2 missing: apt-get install libz2-dev])])
AC_CHECK_LIB([x264], [x264_encoder_headers], [], [AC_MSG_ERROR([-lx264 missing: apt-get install libx264])])
AC_CHECK_LIB([pthread], [pthread_create], [], [AC_MSG_ERROR([-lpthread missing? wtf?])])
AC_CHECK_LIB([avutil], [av_log], [], [AC_MSG_ERROR([-lavutil missing: install ffmpeg/avconv libraries])])
AC_CHECK_LIB([avcodec], [av_log], [], [AC_MSG_ERROR([-lavcodec missing: install ffmpeg/avconv libraries])])
AC_CHECK_LIB([z], [inflate], [], [AC_MSG_ERROR([-lz missing? wtf?])])
AC_CHECK_LIB([avformat], [av_log], [], [AC_MSG_ERROR([-lavformat missing: install ffmpeg/avconv libraries])])

AC_CHECK_HEADERS([sys/socket.h], [], [AC_MSG_ERROR(["sys/socket.h: you don't have enough development headers"])])
AC_CHECK_HEADERS([sys/types.h], [], [AC_MSG_ERROR(["sys/types.h: you don't have enough development headers"])])
AC_CHECK_HEADERS([netinet/in.h], [], [AC_MSG_ERROR(["arpa/inet.h: you don't have enough development headers"])])
AC_CHECK_HEADERS([arpa/inet.h], [], [AC_MSG_ERROR(["arpa/inet.h: you don't have enough development headers"])])
AC_CHECK_HEADERS([signal.h], [], [AC_MSG_ERROR(["signal.h: you don't have enough development headers"])])
AC_CHECK_HEADERS([stdio.h], [], [AC_MSG_ERROR(["stdio.h: you don't have enough development headers"])])
AC_CHECK_HEADERS([stdlib.h], [], [AC_MSG_ERROR(["stdlib.h: you don't have enough development headers"])])
AC_CHECK_HEADERS([string.h], [], [AC_MSG_ERROR(["string.h: you don't have enough development headers"])])
AC_CHECK_HEADERS([unistd.h], [], [AC_MSG_ERROR(["unistd.h: you don't have enough development headers"])])
AC_CHECK_HEADERS([pthread.h], [], [AC_MSG_ERROR(["pthread.h: you don't have enough development headers"])])
AC_CHECK_HEADERS([sys/time.h], [], [AC_MSG_ERROR(["sys/time.h: you don't have enough development headers"])])
AC_CHECK_HEADERS([fcntl.h], [], [AC_MSG_ERROR(["fcntl.h: you don't have enough development headers"])])
AC_CHECK_HEADERS([sys/mman.h], [], [AC_MSG_ERROR(["sys/mman.h: you don't have enough development headers"])])
AC_CHECK_HEADERS([assert.h], [], [AC_MSG_ERROR(["assert.h: you don't have enough development headers"])])
AC_CHECK_HEADERS([libavcodec/avcodec.h], [], [AC_MSG_ERROR([please install the ffmpeg/avconf libraries and headers])])
AC_CHECK_HEADERS([libavformat/avformat.h], [], [AC_MSG_ERROR([please install the ffmpeg/avconf libraries and headers])])
AC_CHECK_HEADERS([libavutil/opt.h], [], [AC_MSG_ERROR([please install the ffmpeg/avconf libraries and headers])])
AC_CHECK_HEADERS([libswscale/swscale.h], [], [AC_MSG_ERROR([please install the ffmpeg/avconf libraries and headers])])
AC_CHECK_HEADERS([xdo.h], [], [AC_MSG_ERROR([xdo.h missing: apt-get install libxdo-dev])])

AC_CHECK_FUNCS([bzero gettimeofday inet_ntoa select socket], [], [AC_MSG_ERROR(["you don't have enough development headers"])])

AC_FUNC_MALLOC
AC_TYPE_INT32_T
AC_TYPE_INT64_T
AC_TYPE_UINT64_T
AC_TYPE_UINT8_T 
AC_TYPE_SIZE_T

#files to generate via autotools (prepare .am or .in source files)
AC_CONFIG_FILES([Makefile])

# finally this generates the Makefiles etc. for the build
AC_OUTPUT

