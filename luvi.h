#ifndef FARM_H
#define FARM_H
#include <sys/socket.h> 
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/time.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/opt.h>
#include <libswscale/swscale.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <netdb.h>
#include <xdo.h>
//#define NDEBUG
#include <assert.h>
#include "luvi.pb-c.h"

#define USEC(x)         (ulong) (x.tv_sec * 1000000 + x.tv_usec)

/*
 * These contansts are totally empirical, and should probably include
 * some code in the future for dynamic adjustability.
 */

#define MAX_SLAVES 		10		// arbitrary, can be increased
#define MAX_PACKET 		1000000		// maximum I-frame size
#define MAX_DUPS 		10		// maximum duplicated packets between timebases
#define QUEUESIZE 		4000 		// maximum outstanding I/O to slaves
#define RANGESIZE 		10		// maximum GOPs per slave to transcode (Range) 
#define MAX_EXPECTED_GOP_SIZE	300 		// maximum frames per GOP
#define MAX_STAGING	 	32*1024*1024	// not used	
#define NETWORK_BUFFER		1000000		// setsockopt buffer size
#define MAX_MSG_SIZE 		4096		// maximum protocol buffer size

// I'm not using encode_video2(), yet. Stop yelling at me.
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"

static AVRational myAVTIMEBASEQ = {1, AV_TIME_BASE};
static int debug = 3;

typedef enum {
	INIT = 0,
	CODEC = 1,
	TRANSCODE = 2,
	TRANSCODE_RESULT = 3,
	CMDLEN = 4,
} command_code_t;

static char * describe[] = {
	"initialize",
	"codec",
	"transcode",
	"transcode_result",
};

typedef struct QUEUE queue;

/*
 * Data structures for slave workers.
 */
typedef struct {
	int	fd;
	int	id;
	int	ranges;
	int	busy;
	queue	* results;
	char	ip[30];
	char 	name[100]; uint8_t sbuf[MAX_MSG_SIZE];
	uint8_t rbuf[MAX_MSG_SIZE];
} slave_t;

/*
 * Each queue entry represents the transcoding
 * of a single video frame.
 */
typedef struct {
	/*
	 * Pre-caculated timing parameters of this frame sent to the slaves.
	 */
	int 		got_packet;
	int		fd;
	int		type;
	float 		inPts;
	float 		outFrames;
	float 		vdelta;
	int64_t 	m_pts;
	int32_t 	current_frame_number;

	/*
	 * The resulting meta-data of the transcoded frame,
	 * including any duplicate frames that may result.
	 */
	int64_t 	pts[MAX_DUPS];
	int64_t 	dts[MAX_DUPS];
	int32_t 	flags[MAX_DUPS];
	int32_t 	buffer_lengths[MAX_DUPS];

	/*
         * Temporary variables used during transcoding.
	 */
	AVPacket	save_packet;  
	Packet		packet;  
	uint8_t       * outBuffers[MAX_DUPS];

	/*
	 * Structures used for communicating with slaves workers.
	 */
	Config 		configuration;
	Convert 	convert;
	Command 	cmd;
	Values 		values;
	Command       * recv;
	slave_t       * slave;
} queue_entry_t;

#define ENTRYSIZE sizeof(queue_entry_t)

/*
 * Multi-threaded producer/consumer.
 */
struct QUEUE {
        queue_entry_t 	** cmds;
        long 		head, 
			tail;
        int 		full, 
			empty;
        pthread_mutex_t * mut;
        pthread_cond_t  * notFull, 
			* notEmpty;
	int		max;
	int		size;
};

/*
 * Private variables used for FFMPEG interactions,
 * including slave management, codec management,
 */ 
typedef struct {
	Config		  configuration;
	pthread_t	  in_consumer;
	pthread_t	  out_consumer;
	pthread_t	  listener;
	int64_t		  start_time;
	int		  processors;
	int		  audioStream;
	int		  videoStream;
	int 		  stop_in_consumer;
	int 		  stop_out_consumer;
	int 		  stop_listener;
	int		  ready_for_slaves;
	int		  drops_stopped;
	int		  encoder_clean;
	int		  first_keyframe_reached;
	int		  remote;
	int		  sockd;
	int		  stagefd;
	void		* staging;
	int		  num_slaves;
	int		  ranges;
	double		  fps;
	char		  hostname[100];
	xdo_t		* xdo;
	slave_t		  slaves[MAX_SLAVES];
	slave_t		* curr_slave;
        queue 		* in_fifo;
        queue 		* out_fifo;
	AVFormatContext * inFormatCtx;
	AVFormatContext * outFormatCtx;
	AVCodecContext  * outVideoCodecCtx;
	AVCodecContext  * inVideoCodecCtx;
	AVCodecContext  * outAudioCodecCtx;
	AVCodecContext  * inAudioCodecCtx;
	AVCodec 	* outVideoCodec;
	AVCodec         * inVideoCodec;
	AVCodec 	* outAudioCodec;
	AVCodec         * inAudioCodec;
	AVFrame         * outVideoFrame;
	AVFrame         * inVideoFrame;
	AVStream 	* inVideoStream;
	AVStream 	* inAudioStream;
	AVStream        * outVideoStream;
	AVStream        * outAudioStream;
	AVPacket          inPacket;
	AVPicture 	  outPicture;
} Context;

/*
 * FFMPEG initializtion routines,
 * for both the input codec and the
 * output codecs.
 */
int 		init_ctx(Context * ctx, int queuesize);
int 		init_in(Context * ctx, int slave);
int 		init_out(Context * ctx, int slave);
void 		init_codec(Context * ctx, AVCodecContext * outVideoCodecCtx, int slave);
void 		destroy_ctx_in(Context * ctx);
void 		destroy_ctx_out(Context * ctx);
void 		destroy_ctx(Context * ctx);

/*
 * Actual transcoding routines
 */
int 		transcode(Context * ctx, queue_entry_t * e, Convert * convert, AVPacket * packet);
void 		packet_to_protobuf(Packet * dest, AVPacket * src);
void 		protobuf_to_packet(AVPacket * dest, Packet * src);
void 		store_transcode_result(Command * send, queue_entry_t * e);

/*
 * Networking helper functions for non-blocking sockets
 * and management of protocol with slaves.
 */
int 		cmdsend(int fd, void * buf, Command * cmd);
int 		cmdrecv(int fd, void * buf, Command ** cmd, int32_t expecting);
int 		multirecv(int fd, uint8_t * buffer, int size);
int 		multiwrite(int fd, uint8_t * packet_data, int remaining);

/*
 * FIFO queue management.
 */
queue 	      * queueInit (int queuesize);
void 		queueDelete (queue *q);
void 		queueAdd (queue *q, queue_entry_t * in);
void 		queueDel (queue *q, queue_entry_t **out);
void 		push(queue * fifo, queue_entry_t * e);
queue_entry_t * pop(queue * fifo, int * stop);
queue_entry_t * queueHead(queue *q);
void 		in_consumer_stop(Context * ctx);
void 		out_consumer_stop(Context * ctx);

struct addrinfo * gethost(char * name);
void update_fps(Context * ctx, int init);

#endif
