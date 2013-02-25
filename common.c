#include "luvi.h"

/*
 * Copy the parameters of the original video stream to the output codec.
 */
void init_codec(Context * ctx, AVCodecContext * CodecCtx, int slave)
{
	CodecCtx->bit_rate              	= ctx->configuration.video_bit_rate;
	CodecCtx->width                 	= ctx->configuration.width;
	CodecCtx->height                	= ctx->configuration.height;
	CodecCtx->pix_fmt               	= PIX_FMT_YUV420P;
	CodecCtx->sample_aspect_ratio.num   	= ctx->configuration.sample_aspect_ratio_num;
	CodecCtx->sample_aspect_ratio.den   	= ctx->configuration.sample_aspect_ratio_den;
	CodecCtx->time_base.num         	= ctx->configuration.avg_frame_rate_den;
	CodecCtx->time_base.den         	= ctx->configuration.avg_frame_rate_num;
}

struct addrinfo * gethost(char * name)
{
	struct addrinfo       * info, * result;
	int s;

	s = getaddrinfo(name, NULL, NULL, &result);

	if(s != 0 || result == NULL) {
		perror("getaddrinfo");
		printf("Failed to lookup address: %s\n", name);
		exit(1);
	}

	return result;
}

/*
 * Open the output codec, supporting frame and configure it to our liking.
 */
int init_out(Context * ctx, int slave)
{
	if(slave == 0) {
		//char * container = "mpegts";
		char * container = "matroska";

		if(avformat_alloc_output_context2(&ctx->outFormatCtx, NULL, container, NULL) < 0) {
			printf("Could not allocate output format context %s\n", container);
			return -1;
		}

		if(!ctx->outFormatCtx) {
			printf("Could not find suitable output format context %s\n", container);
			return -1;
		}
	}

	/* find the encoder */
	ctx->outVideoCodec = avcodec_find_encoder(AV_CODEC_ID_H264);
	if (!(ctx->outVideoCodec)) {
	     printf("Could not find video codec\n");
		return -1;
	}

	if(ctx->configuration.audio_codec_id != -1) {
		/* find the encoder */
		ctx->outAudioCodec = avcodec_find_encoder(ctx->configuration.audio_codec_id);
		if (!(ctx->outAudioCodec)) {
		     printf("Could not find audio codec\n");
			return -1;
		}
	}

	if(slave) {
		ctx->outVideoCodecCtx = avcodec_alloc_context3(ctx->outVideoCodec);

		if(ctx->outVideoCodecCtx == NULL) {
			printf("Failed to open output codec context\n");
			return -1;
		}

	} else {
		ctx->outVideoStream = avformat_new_stream(ctx->outFormatCtx, ctx->outVideoCodec);
		if (!ctx->outVideoStream) {
			printf("Could not allocate output video stream\n");
			return -1;
		}

		ctx->outVideoStream->id = ctx->outFormatCtx->nb_streams - 1;
		printf("Output video stream id: %d\n", ctx->outVideoStream->id);

		ctx->outVideoCodecCtx = ctx->outVideoStream->codec;

		if(ctx->configuration.audio_codec_id != -1) {
			ctx->outAudioStream = avformat_new_stream(ctx->outFormatCtx, ctx->outAudioCodec);
			if (!ctx->outAudioStream) {
				printf("Could not allocate stream\n");
				return -1;
			}

			/* Consider copying *all* the audio streams and subtitles.... */

			ctx->outAudioStream->id = ctx->outFormatCtx->nb_streams-1;
			ctx->outAudioCodecCtx = ctx->outAudioStream->codec;

			 printf("Output audio stream id: %d\n", ctx->outAudioStream->id);

			avcodec_copy_context(ctx->outAudioCodecCtx, ctx->inAudioStream->codec);

			av_dict_set(&ctx->outAudioStream->metadata, "language", 
				av_dict_get(ctx->inAudioStream->metadata, "language", NULL, 0)->value, 0);

			ctx->outAudioCodecCtx->sample_rate = ctx->configuration.audio_sample_rate;
			ctx->outAudioCodecCtx->bit_rate = ctx->configuration.audio_bit_rate;
			ctx->outAudioCodecCtx->sample_fmt = ctx->configuration.audio_sample_format;
			ctx->outAudioCodecCtx->frame_size = ctx->configuration.audio_frame_size;
			ctx->outAudioCodecCtx->channels = ctx->configuration.audio_channels;
			ctx->outAudioCodecCtx->audio_service_type = ctx->configuration.audio_service_type;
		}
	}

	/*
	 * Mmmmm.... let's make a bluray disc.
	 */

	/*
         *
	 * THESE SETTINGS WORK LOCAL. TESTED IN PLAYER>
         *
	avcodec_get_context_defaults3(ctx->outVideoCodecCtx, ctx->outVideoCodec);
	av_opt_set(ctx->outVideoCodecCtx->priv_data, "tune", "zerolatency", 0);
	av_opt_set(ctx->outVideoCodecCtx->priv_data, "preset", "ultrafast", 0);
	av_opt_set(ctx->outVideoCodecCtx->priv_data, "x264opts", "bitrate=7000:bluray-compat=1:tff=1:vbv_maxrate=40000:vbv_bufsize=30000:keyint=30:open-gop=1:fake-interlaced=1:slices=4:colorprim=bt709:transfer=bt709:colormatrix=bt709:level=4.1", 0);
	 */

	/*
	 * DEVELOPMENT SETTINGS
         */
	avcodec_get_context_defaults3(ctx->outVideoCodecCtx, ctx->outVideoCodec);
	av_opt_set(ctx->outVideoCodecCtx->priv_data, "tune", "zerolatency", 0);
	av_opt_set(ctx->outVideoCodecCtx->priv_data, "preset", "ultrafast", 0);
	av_opt_set(ctx->outVideoCodecCtx->priv_data, "x264opts", "bitrate=7000:bluray-compat=1:tff=1:vbv_maxrate=40000:vbv_bufsize=30000:keyint=30:open-gop=1:fake-interlaced=1:slices=4:colorprim=bt709:transfer=bt709:colormatrix=bt709:level=4.1", 0);


        init_codec(ctx, ctx->outVideoCodecCtx, slave);

	/* open the codec */
	if (avcodec_open2(ctx->outVideoCodecCtx, ctx->outVideoCodec, NULL) < 0) {
	     printf("Could not open output video codec\n");
	     return -1;
	}

	printf("Opened output video codec %s\n", avcodec_get_name(ctx->outVideoCodecCtx->codec_id));

	ctx->outVideoFrame = avcodec_alloc_frame();
	if (!ctx->outVideoFrame) {
		printf("Could not allocate video frame\n");
		return -1;
	}

	// Determine required buffer size and allocate buffer
	if(avpicture_alloc(&ctx->outPicture, ctx->outVideoCodecCtx->pix_fmt, ctx->outVideoCodecCtx->width, ctx->outVideoCodecCtx->height) < 0) {
		printf("Could not allocate output picture\n");
		return -1;	
	}

	*((AVPicture *)ctx->outVideoFrame) = ctx->outPicture;

        return 0;
}

/*
 * Open the input codec based on the ID used by the source file. 
 */
int init_in(Context * ctx, int slave)
{
        int x = 0;
	// Find the decoder for the video stream
	ctx->inVideoCodec = avcodec_find_decoder(ctx->configuration.video_codec_id);

	if(!ctx->inVideoCodec) {
		printf("Unsupported video codec!\n");
		return -1;
	}

        ctx->inVideoCodecCtx = avcodec_alloc_context3(ctx->inVideoCodec);

        if(!ctx->inVideoCodecCtx) {
                printf("Failed to allocate video codec context\n");
                return -1;
        }

	// Open codec
	if(avcodec_open2(ctx->inVideoCodecCtx, ctx->inVideoCodec, NULL) < 0) {
            printf("Could not open input video codec\n");
            return -1;
        }

	printf("Opened input video codec %s\n", avcodec_get_name(ctx->inVideoCodecCtx->codec_id));

	ctx->inVideoFrame = avcodec_alloc_frame();

        return 0;
}

void destroy_ctx(Context * ctx)
{
	int x;

	if(ctx->xdo)
		xdo_free(ctx->xdo);

	for(x = 0; x < MAX_SLAVES; x++)
		queueDelete(ctx->slaves[x].results);

	queueDelete (ctx->in_fifo);
	queueDelete (ctx->out_fifo);

	destroy_ctx_out(ctx); 
	destroy_ctx_in(ctx);

	free(ctx);
}
/*
 * Default settings for private master/slave variables.
 */
int init_ctx(Context * ctx, int queuesize)
{
	int x;

	if(gethostname(ctx->hostname, 100) != 0) {
		perror("gethostname");
		printf("Failed to get my own hostname =(");
		return -1;
	}

	config__init(&ctx->configuration);
	
	ctx->configuration.audio_codec_id = -1;

	for(x = 0; x < MAX_SLAVES; x++) {
		ctx->slaves[x].ranges = 0;
		ctx->slaves[x].busy = 0;
		ctx->slaves[x].results = queueInit (QUEUESIZE);
	}

	ctx->fps = 0;
	ctx->videoStream = -1;
	ctx->audioStream = -1;
	ctx->xdo = xdo_new(":0.0");
	if(ctx->xdo)
		xdo_keysequence(ctx->xdo, CURRENTWINDOW, "Shift_L", 0);
	ctx->curr_slave = NULL;
	ctx->num_slaves = 0;
	ctx->ready_for_slaves = 0;
	//ctx->ready_for_slaves = 1;
	ctx->drops_stopped = 0;
	//ctx->drops_stopped = 1;
	//ctx->encoder_clean = 0;
	ctx->encoder_clean = 1;

	ctx->stop_in_consumer = 0;
	ctx->stop_out_consumer = 0;
	ctx->stop_listener = 0;
	ctx->first_keyframe_reached = 0;
	av_register_all();

	ctx->in_fifo = queueInit (queuesize);
        if (ctx->in_fifo == NULL) {
                printf("main: Input Queue Init failed.\n");
                exit (1);
        }

	ctx->out_fifo = queueInit (queuesize);
        if (ctx->out_fifo == NULL) {
                printf("main: Output Queue Init failed.\n");
                exit (1);
        }

        ctx->outFormatCtx = NULL;
        ctx->inFormatCtx = NULL;
        ctx->outVideoCodecCtx = NULL;
        ctx->inVideoCodecCtx = NULL;
        ctx->outAudioCodecCtx = NULL;
        ctx->outVideoCodec = NULL;
        ctx->inVideoCodec = NULL;
        ctx->outAudioCodec = NULL;
        ctx->outVideoFrame = NULL;
        ctx->inVideoFrame = NULL; 
        ctx->inVideoStream = NULL;
        ctx->outVideoStream = NULL;
        ctx->inAudioStream = NULL;
        ctx->outAudioStream = NULL;

        return 0;
}

/*
 * Get our transcoding game on...
 *
 * This function was designed to be completely parallel.
 * Frames are not sent to the slaves until the decoder becomes
 * deterministic - meaning that the decoder gets primed with some
 * initial packets from the input video stream.
 *
 * Once the decoder starts predictably giving us 1 frame for every
 * input packet, we're good to go and start sending out the packets
 * to the slaves.
 * 
 * For a handful of packets, this function gets called by the master
 * and then we go full blast on all the slaves.
 */
int transcode(Context * ctx, queue_entry_t * e, Convert * convert, AVPacket * packet)
{
	static long 	decode = 0, 
			encode = 0, 
			dups = 0, 
			drops = 0, 
			single = 0, 
			write = 0;
	int 		* buffers = &(convert->buffers),
	 		* extra_frame_count = &(convert->extra_frame_count),
	 		nFrameCount = convert->n_frame_count,
	 		z = 0,
	 		ret = 0,
  	 		frameFinished = 0;
	int64_t 	frame_number = convert->frame_number,
			orig_pts = packet->pts,
			orig_dts = packet->pts;
	struct timeval 	dstart, 
			dstop, 
			ddiff, 
			estart, 
			estop, 
			ediff;

	*extra_frame_count = 0;
	*buffers = 0;

	avcodec_get_frame_defaults(ctx->inVideoFrame);

	if(debug == 3)
		gettimeofday(&dstart, NULL);

	if(avcodec_decode_video2(ctx->inVideoCodecCtx, ctx->inVideoFrame, &frameFinished, packet) < 0) {
		printf("Error decoding dts %" PRId64 "\n", packet->dts);
		exit(1);
	}

	if(debug == 3) {
		gettimeofday(&dstop, NULL);
		timersub(&dstop, &dstart, &ddiff);
		decode += (USEC(ddiff) / 1000);
	}
		
	/*
 	 * If we're not done yet (should only happen in the beginning),
	 * Tell the master.
	 */
	if(!frameFinished) {
	     if(debug == 2)
	     printf("skipping pts %" PRId64 " dts %" PRId64 "\n", packet->pts, packet->dts);
	     return 0;
	} else {
		if(debug == 2)
		printf("finished pts %" PRId64 " dts %" PRId64 "\n", orig_pts, orig_dts);
	}

	/*
 	 * Tally the dropped and duplicated frames.
	 */
	if (nFrameCount == 0) {
		drops++;
	} else if (nFrameCount > 1) {
		dups += (nFrameCount - 1);
		single++;
	} else {
		single++;
	}

	/*
	 * Copy the decoded frame into a new frame for encoding.
	 * 
	 * FIXME: Is this necessary? Can I juse re-encode the original?
	 */
	av_picture_copy((AVPicture*)ctx->outVideoFrame,(AVPicture*)ctx->inVideoFrame,ctx->outVideoCodecCtx->pix_fmt, ctx->outVideoCodecCtx->width,ctx->outVideoCodecCtx->height);

	/*
	 * Encode, taking duplicates into account.
	 */
	for (z = 0; z < nFrameCount && z < 10; z++)
	{
		if(debug==2)
			printf("INpkt Frame %" PRId64 " dts: %" PRId64 " pts: %" PRId64 "\n", frame_number, orig_dts, orig_pts);

		/*
		 * Sometimes the encoder doesn't finish in one sitting...
		 */
		do {
		    ctx->outVideoFrame->pts = frame_number;
		    ctx->outVideoFrame->pict_type = 0;

		    if(debug == 3)
			    gettimeofday(&estart, NULL);

		    e->outBuffers[z] = av_malloc(MAX_PACKET);
		    assert(e->outBuffers[z]);
		    ret = avcodec_encode_video(ctx->outVideoCodecCtx, e->outBuffers[z], MAX_PACKET, ctx->outVideoFrame);   
		    
		    if(debug == 3) {
			    gettimeofday(&estop, NULL);
			    timersub(&estop, &estart, &ediff);
			    encode += (USEC(ediff) / 1000);
		    }

		    if(ret < 0) {
			printf("Error encoding frame\n");
			exit(1);
		    }
		    if(ret == 0) {
			    /* 
			     *	If the encoder is not yet deterministic either,
			     *  then take note of it.
			     */
			    *extra_frame_count = *extra_frame_count + 1;
			    frame_number++;
			    if(debug==2)
			    printf("not finished, bumping frame_number to %" PRId64 "\n", frame_number);
		    }
		} while(ret == 0);
	    
		if (ret > 0) {
		    /*
		     * Save all the meta-data of the newly transcoded frame for transmission to master.
		     */
		    e->buffer_lengths[z] = ret;
		    e->flags[z] = 0;
		    e->pts[z] = AV_NOPTS_VALUE;
		    e->dts[z] = AV_NOPTS_VALUE;

		    if(ctx->outVideoCodecCtx->coded_frame && ctx->outVideoCodecCtx->coded_frame->key_frame)
				e->flags[z] |= AV_PKT_FLAG_KEY;

		    /*
		     * Proper time-base conversion.
		     */
		    if (ctx->outVideoCodecCtx->coded_frame && ctx->outVideoCodecCtx->coded_frame->pts != AV_NOPTS_VALUE)
		    {
			    AVRational stream_time_base = {.num = ctx->configuration.stream_time_base_num, .den = ctx->configuration.stream_time_base_den };
			    e->pts[z] = av_rescale_q(ctx->outVideoFrame->pts, ctx->outVideoCodecCtx->time_base, stream_time_base);
			    if(debug == 2)
			    printf("MUX: Rescaled video PTS from %" PRId64 " (codec time_base) to %" PRId64 " (stream time_base) outframe pts %" PRId64 " frame number %" PRId64 ".\n", ctx->outVideoCodecCtx->coded_frame->pts, e->pts[z], ctx->outVideoFrame->pts, frame_number);
		    }

		    /* FIXME: Is this correct? */
		    e->dts[z] = e->pts[z];

		    if(debug == 2)
		    printf("MUX: (%" PRId64 ") queue %d, Writing video frame with PTS: %" PRId64 " DTS %" PRId64 ".\n", frame_number, ctx->in_fifo->size, e->pts[z], e->dts[z]);

		    if((frame_number % (int) ctx->configuration.avg_framerate) == 0) {
			    /*
			     * Nice library to make sure the slave computer does not go to sleep while we are
			     * transcoding by simulating a harmless keystroke on the display.
			     */
			    if(ctx->xdo && ((frame_number % 10) == 0)) {
				    printf("\nEmitting shift keypress to keep machine alive\n");
				    xdo_keysequence(ctx->xdo, CURRENTWINDOW, "Shift_L", 0);
			    }

			    printf("\r(%1.1f %%) (%" PRId64 ") queue %d dups %ld single %ld drops %ld", 
					(frame_number / ctx->configuration.expected_total_frames) * 100, 
					frame_number,
					ctx->in_fifo->size,
					dups, single, drops);
			    printf(" (size=%5d) pts %" PRId64 " dts %" PRId64 " ", e->buffer_lengths[z], e->pts[z], e->dts[z]);
			    printf(" decode %ld encode %ld fps %f =======", decode, encode, ctx->fps);
			    fflush(stdout);
		    }

		} 

		frame_number++;
	}

	*buffers = z;
}

/*
 * Close everything properly to get resulting x264 statistics.
 */
void destroy_ctx_in(Context * ctx) 
{
	avcodec_free_frame(&ctx->inVideoFrame);
	if(ctx->inVideoCodecCtx)
		avcodec_close(ctx->inVideoCodecCtx);
}

void destroy_ctx_out(Context * ctx) 
{
	if(ctx->outVideoCodecCtx)
		avcodec_close(ctx->outVideoCodecCtx);
	if(ctx->outAudioCodecCtx)
		avcodec_close(ctx->outAudioCodecCtx);
	av_free(ctx->outFormatCtx);
	avcodec_free_frame(&ctx->outVideoFrame);
}

/*
 * Send/Receive functions for transmitting 
 * using protocol buffers.
 */

int cmdsend(int fd, void * buf, Command * cmd) 
{
	char 	      *	desc = describe[cmd->code];
	int 		ret;
	unsigned short 	len = command__get_packed_size(cmd), 
			nlen = htons(len);

	if(len > MAX_MSG_SIZE) {
		printf("send protobuf is too big for buffer %u: %s fd %d\n", len, desc, fd);
		exit(1);
	}

	ret = multiwrite(fd, (uint8_t*) &nlen, sizeof(nlen));
	if(ret <= 0) {
		printf("Failed to send protobuf length %u: %s fd %d\n", len, desc, fd);
		perror("send");
		exit(1);
	}

	command__pack(cmd, buf);
	ret = multiwrite(fd, buf, len);

	if(ret <= 0) {
		printf("Failure: send: %s fd %d\n", desc, fd);
		perror("send");
		exit(1);
	}

	if(ret != len) {
		printf("Failure send fd %d: %s is too small: %d\n", fd, desc, ret);
		exit(1);
	}


	return ret;
}

/*
 * Taking care to handle incomplete transmissions and non-blocking I/O.
 */

int multiwrite(int fd, uint8_t * packet_data, int remaining)
{
	int total = 0;
	int ret;

	do {
		ret = write(fd, packet_data + total, remaining);

		if(ret == -1 && errno == EAGAIN) {
			fd_set wait_for_write;
			FD_ZERO(&wait_for_write);
			FD_SET(fd, &wait_for_write);
			ret = select(fd + 1, NULL, &wait_for_write, NULL, NULL);
			if(ret < 0) {
				printf("failed to deail with non blocking write!\n");
				return ret;
			}

			ret = write(fd, packet_data + total, remaining);
		}

		if(ret < 0) {
			perror("multiwrite packet");
			printf("Failed to write packet data %d\n", ret);
			return ret;
		}
		if(ret == 0) {
			perror("multiwrite packet");
			printf("disconnected multiwrite\n");
			return 0;
		}
		remaining -= ret;
		total += ret;
	} while(remaining > 0);

	return total;
}

int multirecv(int fd, uint8_t * packet_data, int remaining)
{
	int total = 0;
	int ret;

	do {
		ret = read(fd, packet_data + total, remaining);

		if(ret == -1 && errno == EAGAIN) {
			fd_set wait_for_read;
			FD_ZERO(&wait_for_read);
			FD_SET(fd, &wait_for_read);
			ret = select(fd + 1, &wait_for_read, NULL, NULL, NULL);
			if(ret < 0) {
				printf("failed to deal with non blocking read!\n");
				return ret;
			}

			ret = read(fd, packet_data + total, remaining);
		}
		if(ret < 0) {
			perror("multirecv packet");
			printf("Failed to receive packet data %d\n", ret);
			return ret;
		}
		if(ret == 0) {
			perror("multirecv packet");
			printf("disconnected multirecv\n");
			return 0;
		}
		remaining -= ret;
		total += ret;
	} while(remaining > 0);

	return total;
}

int cmdrecv(int fd, void * buf, Command ** dest, int32_t expecting) {
	char * expecting_desc = describe[expecting];
	int ret;
	unsigned short len, nlen;
	Command * cmd;
	
	ret = multirecv(fd, (uint8_t *) &nlen, sizeof(nlen));

	if(ret <= 0) {
		printf("Failure: recv: could not determine command length %s\n", expecting_desc);
		perror("recv");
		exit(1);
	}

	len = ntohs(nlen);

//	printf("Receiving protobuf of length: %u\n", len);

	if(len > MAX_MSG_SIZE) {
		printf("receive protobuf is too big for buffer %u: %s fd %d\n", len, expecting_desc, fd);
		exit(1);
	}

	ret = multirecv(fd, buf, len);

	if(ret <= 0) {
		printf("cmdrecv multirecv failure: %s\n", expecting_desc);
		exit(1);
	}

	if(ret != len) {
		printf("Failure recv: %s is too small: %d\n", expecting_desc, ret);
		exit(1);
	}

	cmd = command__unpack(NULL, len, buf);

	if(cmd == NULL) {
		printf("Failed to unpack protocol buffer: expecting %s (%d) len %d\n", expecting_desc, expecting, len);
		exit(1);
	}

	if(cmd->code < 0 || cmd->code >= CMDLEN) {
		printf("Received Command code is corrupted: %d, expecting %s (%d)\n", cmd->code, expecting_desc, expecting);
		exit(1);
	}

	if(expecting != cmd->code) {
		char * desc = describe[cmd->code];
		printf("recv: Was expecting %s (%d) but got %s (%d) instead. Bailing.\n", expecting_desc, expecting, desc, cmd->code);
		exit(1);
	}	

	*dest = cmd;

	return ret;
}

#define COPY_PACKET_VALUES(dest, src) 					\
	do {								\
		dest->pts = src->pts;					\
		dest->dts = src->dts;					\
		dest->size = src->size;					\
		dest->stream_index = src->stream_index;			\
		dest->flags = src->flags;				\
		dest->duration = src->duration;				\
		dest->pos = src->pos;					\
		dest->convergence_duration = src->convergence_duration; \
	} while(0)				
	
void packet_to_protobuf(Packet * dest, AVPacket * src)
{
	COPY_PACKET_VALUES(dest, src);
}
void protobuf_to_packet(AVPacket * dest, Packet * src)
{
	COPY_PACKET_VALUES(dest, src);
}

void store_transcode_result(Command * send, queue_entry_t * e)
{
	if(!send->convert->buffers)
		return;

	send->convert->values = &e->values;
	values__init(send->convert->values);
	send->convert->values->n_pts = send->convert->buffers;
	send->convert->values->n_dts = send->convert->buffers;
	send->convert->values->n_buffer_lengths = send->convert->buffers;
	send->convert->values->n_flags = send->convert->buffers;

	send->convert->values->pts = e->pts;
	send->convert->values->dts = e->dts;
	send->convert->values->buffer_lengths = e->buffer_lengths;
	send->convert->values->flags = e->flags;
}

/*
 * Multi-threaded queue management routines.
 */
queue *queueInit (int queuesize)
{
        queue *q;

        q = (queue *) malloc (sizeof (queue));
        if (q == NULL) return (NULL);

	q->max = queuesize;
	q->cmds = malloc(sizeof(queue_entry_t) * q->max);
        q->empty = 1;
        q->full = 0;
        q->head = 0;
        q->tail = 0;
	q->size = 0;
        q->mut = (pthread_mutex_t *) malloc (sizeof (pthread_mutex_t));
        pthread_mutex_init (q->mut, NULL);
        q->notFull = (pthread_cond_t *) malloc (sizeof (pthread_cond_t));
        pthread_cond_init (q->notFull, NULL);
        q->notEmpty = (pthread_cond_t *) malloc (sizeof (pthread_cond_t));
        pthread_cond_init (q->notEmpty, NULL);
        
        return (q);
}

void queueDelete (queue *q)
{
        pthread_mutex_destroy (q->mut);
        free (q->mut);  
        pthread_cond_destroy (q->notFull);
        free (q->notFull);
        pthread_cond_destroy (q->notEmpty);
        free (q->notEmpty);
	free (q->cmds);
        free (q);
}

void queueAdd (queue *q, queue_entry_t * in)
{
        q->cmds[q->tail] = in;
        q->tail++;
        if (q->tail == q->max)
                q->tail = 0;
        if (q->tail == q->head)
                q->full = 1;
        q->empty = 0;
	q->size++;
}

queue_entry_t * queueHead(queue *q) {
	if(q->empty)
		return NULL;
	return q->cmds[q->head];
}

void queueDel (queue *q, queue_entry_t ** out)
{
        *out = q->cmds[q->head];

	q->cmds[q->head] = NULL;
        q->head++;
        if (q->head == q->max)
                q->head = 0;
        if (q->head == q->tail)
                q->empty = 1;
        q->full = 0;
	q->size--;
}

void push(queue * fifo, queue_entry_t * e)
{
	pthread_mutex_lock(fifo->mut);
	while(fifo->full) {
		printf("\nQueue is full %d max %d\n", fifo->size, fifo->max);
		pthread_cond_wait(fifo->notFull, fifo->mut);
		printf("\nNot full anymore... %d max %d\n", fifo->size, fifo->max);
	}
	queueAdd(fifo, e);
	pthread_mutex_unlock(fifo->mut);
	pthread_cond_signal(fifo->notEmpty);
}

void in_consumer_stop(Context * ctx)
{
	  ctx->stop_in_consumer = 1;
	  pthread_cond_signal(ctx->in_fifo->notEmpty);
          pthread_join(ctx->in_consumer, NULL);
}

void out_consumer_stop(Context * ctx)
{
	  ctx->stop_out_consumer = 1;
	  pthread_cond_signal(ctx->out_fifo->notEmpty);
          pthread_join(ctx->out_consumer, NULL);
}

queue_entry_t * pop(queue * fifo, int * stop) 
{
	queue_entry_t * e;
	pthread_mutex_lock (fifo->mut);
	while (fifo->empty) {
		if(stop && *stop) {
			pthread_mutex_unlock (fifo->mut);
			return NULL;
		}

		pthread_cond_wait (fifo->notEmpty, fifo->mut);
	}

	queueDel(fifo, &e);
	pthread_mutex_unlock (fifo->mut);
	pthread_cond_signal (fifo->notFull);
	return e;
}

void update_fps(Context * ctx, int init)
{
	static struct timeval start, stop, diff;
	static long frames = 0, oldframes = 0;

	if(init) {
		gettimeofday(&start, NULL);
		return;
	}

	frames++;

	if((frames % 1000) == 0) {
		gettimeofday(&stop, NULL);
		timersub(&stop, &start, &diff);
		if(diff.tv_sec > 0) {
			double time = ((double) USEC(diff)) / 1000000.0;
			double total = frames - oldframes; 
			ctx->fps = total / time;
			printf("%f frames per second %f %f.\n", 
				ctx->fps,
				time,
				total);
		} else {
			printf("differences is zero.\n");
		}
		oldframes = frames;
		start = stop;
	}
}

