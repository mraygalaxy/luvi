#include "luvi.h"

int abort_requested = 0;

static void my_handler(int s)
{
	printf("\nAbort requested (CTRL-C)\n");
	if(abort_requested) {
		printf("Abort didn't work. Bailing\n");
		exit(1);
	}
	abort_requested = 1;
}


static void my_write_packet(Context * ctx, AVPacket * pkt)
{
//	if(av_interleaved_write_frame(ctx->outFormatCtx, pkt) < 0) {
	if(av_write_frame(ctx->outFormatCtx, pkt) < 0) {
		printf("Could not output packet\n");
		exit(1);
	}
//	avio_flush(ctx->outFormatCtx->pb);
}

static void output_audio(Context * ctx, queue_entry_t * e) 
{

	if(debug==4 || debug == 2)
	printf("Got audio packet @ pts %" PRId64 " dts %" PRId64 "\n", ctx->inPacket.pts, ctx->inPacket.dts);

	e->save_packet.stream_index = ctx->outAudioStream->index;
	e->save_packet.pts -= ctx->start_time;
	e->save_packet.dts = e->save_packet.pts;

	if(e->save_packet.pts >= 0) {
		if(debug==4 || debug == 2)
		printf("MUXAudio: Writing audio frame with PTS: %" PRId64 "\n", e->save_packet.pts);
		my_write_packet(ctx, &e->save_packet);

	} else {
		if(debug==4 || debug == 2)
		printf("MUXAudio: Dropping audio frame with PTS: %" PRId64 "\n", e->save_packet.pts);
	}
	av_free_packet(&e->save_packet);
}

static void output_video(Context * ctx, queue_entry_t * e, Command * cmd)
{
	int x;
	AVPacket outpkt;

	if(cmd->convert->buffers > 0) {
		if(debug==2)
		printf("dts/m_pts %" PRId64 " vdelta %f buffers %d\n", e->m_pts, e->vdelta, cmd->convert->buffers);

		for(x = 0; x < cmd->convert->buffers; x++) {
			av_init_packet(&outpkt);

			outpkt.data = e->outBuffers[x];
			outpkt.size = cmd->convert->values->buffer_lengths[x];
			outpkt.pts = cmd->convert->values->pts[x];
			outpkt.dts = cmd->convert->values->dts[x];
			outpkt.flags = cmd->convert->values->flags[x];

		        if(ctx->remote && (debug==2 || (ctx->fifo->size % 40) == 0)) {
				printf("(%d %%) (%d) Writing from slave %d (%s), ranges %d size %d frame %d total ranges %d queue %d =======\n",  
					(int) (((double) ctx->configuration.frame_number / (double) ctx->configuration.expected_total_frames) * 100), 
					ctx->configuration.frame_number,
					e->slave ? e->slave->id : -1, 
					e->slave ? e->slave->ip : "n/a",
					e->slave ? e->slave->ranges : -1, 
					outpkt.size, 
					ctx->configuration.frame_number, 
					ctx->ranges,
					ctx->fifo->size);

				if(debug==2)
					printf("\n");
				else
					fflush(stdout);
			}

			my_write_packet(ctx, &outpkt);
			av_free(e->outBuffers[x]);	
		}
	} 
}

static void setnonblocking(int sock)
{
	int opts;

	opts = fcntl(sock,F_GETFL);
	if (opts < 0) {
		perror("fcntl(F_GETFL)");
		exit(EXIT_FAILURE);
	}
	opts = (opts | O_NONBLOCK);
	if (fcntl(sock,F_SETFL,opts) < 0) {
		perror("fcntl(F_SETFL)");
		exit(EXIT_FAILURE);
	}
}

static int my_select(Context * ctx, fd_set * socks, int read)
{
	int highest = 0, x, y, check_only = 1, ready;
	struct timeval timeout;

	highest = 0;
	FD_ZERO(socks);

	for(x = 0; x < ctx->num_slaves; x++) {
		int fd = ctx->slaves[x].fd;
		if(fd > highest)
			highest = fd;
		FD_SET(fd, socks);
	}

	timeout.tv_sec = 2;
	timeout.tv_usec = 0;

	if(read)
		ready = select(highest + 1, socks, NULL, NULL, &timeout);
	else
		ready = select(highest + 1, NULL, socks, NULL, &timeout);

	if (ready < 0) {
		if(read)
			perror("select read");
		else
			perror("select write");
		exit(1);
	}

	return ready;
}

static void * disk_writer (void * opaque)
{
	queue_entry_t * e = NULL;
	Context * ctx = opaque;
	int x, y, check_only = 1;
	fd_set socks;
	int ready;

	while(abort_requested == 0) {
		/* Are we still waiting for the head? */
		if(!e) {
			/* No? Get the next one */
			if(!(e = pop(ctx->fifo, &ctx->stop_consumer)))
				break;
		}

	    	if(e->type == ctx->audioStream) {
			output_audio(ctx, e);
			free(e);
			e = NULL;
			continue;
		}

		if(!ctx->remote || !ctx->ready_for_slaves) {
			output_video(ctx, e, &e->cmd);
			free(e);
			e = NULL;
			continue;
		}

		do {
			ready = my_select(ctx, &socks, 1); 
		} while(e == NULL && ready == 0 && abort_requested == 0);

		if(ready) {
			for(y = 0; y < ctx->num_slaves; y++) {
				slave_t * slave = &ctx->slaves[y];
				queue_entry_t * data;
				int fd = slave->fd;

				if(!FD_ISSET(fd, &socks))
					continue;

				if(slave->results->size == (slave->results->max - 1))
					continue;

				data = malloc(ENTRYSIZE);
				assert(data);
				cmdrecv(fd, slave->rbuf, &data->recv, TRANSCODE_RESULT);
				data->slave = slave;
				data->type = ctx->videoStream;

				for(x = 0; x < data->recv->convert->buffers; x++) {
					if(debug==2)
					printf("Receiving %d bytes for slave %d\n", data->recv->convert->values->buffer_lengths[x], slave->id);
					data->outBuffers[x] = av_malloc(data->recv->convert->values->buffer_lengths[x]);
					assert(data->outBuffers[x]);
					if(multirecv(fd, data->outBuffers[x], data->recv->convert->values->buffer_lengths[x]) <= 0) {
						printf("Could not receive %d bytes for slave %d\n", data->recv->convert->values->buffer_lengths[x], slave->id);
						exit(1);
					}
				}

				push(slave->results, data);
				slave->busy--;
			}
		}

		/* 
		 * Check to see if the head matches any of the
		 * data being currently held.
		 */
		for(y = 0; y < ctx->num_slaves; y++) {
			slave_t * slave = &ctx->slaves[y];
			queue_entry_t * head = queueHead(slave->results);

			if(!head)
				continue;

			if(head->type != ctx->videoStream)
				continue;

			if(head->recv->convert->frame_number != e->cmd.convert->frame_number)
				continue;
			
			head = pop(slave->results, &check_only);
			output_video(ctx, head, head->recv);
			command__free_unpacked(head->recv, NULL);
			free(head);
			free(e);
			y = -1;
			do {
				if(!(e = pop(ctx->fifo, &check_only)))
					break;
				if(e->type == ctx->audioStream) {
					output_audio(ctx, e);
					free(e);
					e = NULL;
					continue;
				}
				break;
			} while(1);
		}

        }
	return NULL;
}

void *slave_listener(void * opaque)
{
	Context * ctx = opaque;
	Command cmd;
	int newsockd, size, got;
	struct sockaddr_in saddr;
	int sendbuff = NETWORK_BUFFER, rcvbuff = NETWORK_BUFFER, res;

	size = sizeof(saddr);

	while(!ctx->stop_listener) {
		Command * recv = NULL;
		slave_t * slave;
		printf("== accept\n");
		if((newsockd = accept(ctx->sockd, (struct sockaddr *) &saddr, &size)) < 0) {
			if(ctx->stop_listener) {
				printf("Listener exiting now.\n");
				break;
			}
			perror("accept error");
			exit(1);
		}

		res = setsockopt(newsockd, SOL_SOCKET, SO_SNDBUF, &sendbuff, sizeof(sendbuff));	

		if(res < 0) {
			perror("setsockopt: send buffer:");
			printf("Failed to increase new slave's send buffer\n");
			exit(1);
		}

		res = setsockopt(newsockd, SOL_SOCKET, SO_RCVBUF, &rcvbuff, sizeof(rcvbuff));	

		if(res < 0) {
			perror("setsockopt: receive buffer:");
			printf("Failed to increase new slave's recieve buffer\n");
			exit(1);
		}

		printf("== accepted with fd %d, buffer: %d %d\n", newsockd, sendbuff, rcvbuff);

		slave = &(ctx->slaves[ctx->num_slaves]);
		got = cmdrecv(newsockd, slave->rbuf, &recv, INIT);
		command__free_unpacked(recv, NULL);

		printf("Sending codec id %d\n", ctx->configuration.video_codec_id);

		command__init(&cmd);
		cmd.configuration = &ctx->configuration;
		cmd.code = CODEC;
		got = cmdsend(newsockd, slave->sbuf, &cmd);
		strcpy(slave->ip, inet_ntoa(saddr.sin_addr));
		slave->fd = newsockd;
		slave->id = ctx->num_slaves; 
		ctx->num_slaves++;
		printf("stored fd: %d\n", slave->fd);
		setnonblocking(newsockd);
	}
	return NULL;
}

int main(int argc, char *argv[]) {
	Context		* ctx;
	queue_entry_t   gop_entries[MAX_EXPECTED_GOP_SIZE];
	struct 		sockaddr_in raddr; 
	int64_t 	m_ptsOffset;
	int 		got,
	       		numBytes,
	 		on = 1,
	 		got_output,
	 		x, 
			y,
			rport,
	 		total_packets = 0,
	 		key_frames = 0,
	 		icount = 0,
			gop_length = 0,
			flags;
	char		action_character;
	fd_set 		socks;

	struct sigaction sigIntHandler;

	sigIntHandler.sa_handler = my_handler;
	sigemptyset(&sigIntHandler.sa_mask);
	sigIntHandler.sa_flags = 0;
	sigaction(SIGINT, &sigIntHandler, NULL);

	if(argc < 5) {
		printf("usage: [infile] [outfile] [port] [remote] [slave transcode 0|1]\n");
		return -1;
	}

	ctx = malloc(sizeof(Context));
	rport = atoi(argv[3]);
	ctx->remote = atoi(argv[4]);

	if(init_ctx(ctx, 20000) < 0)
		return -1;

	if(ctx->remote) {
		if ((ctx->sockd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0) {
			perror("socket");
			return -1;
		}

		bzero(&raddr, sizeof(raddr));
		raddr.sin_family      = AF_INET;
		raddr.sin_addr.s_addr = htonl(INADDR_ANY);
		raddr.sin_port        = htons(rport); /* daytime server */

		if (setsockopt(ctx->sockd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) < 0) {
			perror("setsockopt(SO_REUSEADDR)");
			return -1;
		}

		if(bind(ctx->sockd, (struct sockaddr *)(&raddr), sizeof(raddr)) < 0) {
			perror("bind");
			return -1;
		}

		printf("== listen\n");
		if(listen(ctx->sockd, MAX_SLAVES) < 0) {
			perror("listen");
			return -1;
		}

		printf("== listening\n");

		pthread_create (&ctx->listener, NULL, slave_listener, ctx);
	}

	// Open video file
	printf("Opening source: %s\n", argv[1]);
	if(avformat_open_input(&ctx->inFormatCtx, argv[1], NULL, NULL) != 0) {
		perror("avformat_open_input");
		printf("Could not open source video file.\n");
		return -1;
	}

	// Retrieve stream information
	if(avformat_find_stream_info(ctx->inFormatCtx, NULL) < 0) {
		perror("avformat_find_stream_info");
		printf("Could not find stream information\n");
		return -1;
	}

	for(x = 0; x < ctx->inFormatCtx->nb_streams; x++) {
		if(ctx->inFormatCtx->streams[x]->codec->codec_type == AVMEDIA_TYPE_VIDEO) {
			if(ctx->videoStream == -1) {
				ctx->videoStream = x;
				ctx->inVideoStream = ctx->inFormatCtx->streams[x];
				ctx->configuration.video_codec_id = ctx->inFormatCtx->streams[x]->codec->codec_id;
				printf("Found first video stream\n");
			} else
				printf("Ignoring extra video stream %d\n", x);
		} else if(ctx->inFormatCtx->streams[x]->codec->codec_type == AVMEDIA_TYPE_AUDIO) {
			if(ctx->audioStream == -1) {

				ctx->audioStream = x;
				ctx->inAudioStream = ctx->inFormatCtx->streams[x];
				ctx->configuration.audio_codec_id = ctx->inAudioStream->codec->codec_id;
				printf("Found first audio stream\n");
			} else
				printf("Ignoring extra audio stream %d\n", x);
		} else {
			printf("Ignoring unknown stream %d\n", x);
		}
	}

	if(ctx->videoStream == -1) {
		printf("No video streams found.\n");
		return -1;
	}

	if(ctx->audioStream == -1) {
		printf("No audio streams found.\n");
		ctx->configuration.audio_codec_id = -1;
	}

	if(init_in(ctx, 0) < 0)
		return -1;

	ctx->configuration.video_bit_rate = ctx->inFormatCtx->bit_rate;
	ctx->configuration.width = ctx->inVideoStream->codec->width;
	ctx->configuration.height = ctx->inVideoStream->codec->height;
	ctx->configuration.avg_frame_rate_num = ctx->inVideoStream->avg_frame_rate.num;
	ctx->configuration.avg_frame_rate_den = ctx->inVideoStream->avg_frame_rate.den;
	ctx->configuration.sample_aspect_ratio_num = ctx->inVideoStream->codec->sample_aspect_ratio.num;
	ctx->configuration.sample_aspect_ratio_den = ctx->inVideoStream->codec->sample_aspect_ratio.den;
	ctx->configuration.avg_framerate = (float)ctx->configuration.avg_frame_rate_num / (float)ctx->configuration.avg_frame_rate_den;
	ctx->configuration.expected_total_frames = ctx->configuration.avg_framerate * (ctx->inFormatCtx->duration / AV_TIME_BASE);
	ctx->configuration.stream_time_base_num = ctx->inVideoStream->time_base.num;
	ctx->configuration.stream_time_base_den = ctx->inVideoStream->time_base.den;

	if(ctx->audioStream != -1) {
		ctx->configuration.audio_sample_rate = ctx->inAudioStream->codec->sample_rate;
		ctx->configuration.audio_sample_format = ctx->inAudioStream->codec->sample_fmt;
		ctx->configuration.audio_bit_rate = ctx->inAudioStream->codec->bit_rate;
		ctx->configuration.audio_channels = ctx->inAudioStream->codec->channels;
		ctx->configuration.audio_frame_size = ctx->inAudioStream->codec->frame_size;
		ctx->configuration.audio_service_type = ctx->inAudioStream->codec->audio_service_type;
		ctx->configuration.audio_block_align = ctx->inAudioStream->codec->block_align;
	}

	printf("Video:\n");
	printf("Stream avg numerator %d\n", ctx->configuration.avg_frame_rate_num);
	printf("Stream avg denominator %d\n", ctx->configuration.avg_frame_rate_den);
	printf("Average framerate: %f\n", ctx->configuration.avg_framerate);
	printf("Bitrate: %dk\n", ctx->configuration.video_bit_rate / 1000);
	printf("Duration: %ld\n", (long int) (ctx->inFormatCtx->duration / AV_TIME_BASE));
	printf("Expected frames: %f\n", ctx->configuration.expected_total_frames);

	/*
	int64_t res = av_get_int(&ctx->inVideoStream->codec->priv_class, "muxrate", NULL);
	    printf("muxrate set to %"PRId64"\n", res);
	*/

	if(ctx->audioStream != -1) {
		printf("Audio:\n");
		printf("bit rate: %dk\n", ctx->configuration.audio_bit_rate / 1000);
		printf("Sample rate: %dk\n", ctx->configuration.audio_sample_rate / 1000);
		printf("Channels: %d\n", ctx->configuration.audio_channels);
		printf("Format: %s\n", av_get_sample_fmt_name(ctx->configuration.audio_sample_format));
	}
  
	 if(init_out(ctx, 0) < 0)
		return 0;

	 ctx->outVideoStream->sample_aspect_ratio.num = ctx->configuration.sample_aspect_ratio_num;
	 ctx->outVideoStream->sample_aspect_ratio.den = ctx->configuration.sample_aspect_ratio_den;

         if (avio_open(&ctx->outFormatCtx->pb, argv[2], AVIO_FLAG_WRITE) < 0) {
	     printf("Could not open '%s'\n", argv[2]);
	     return -1;
	 }

	/* Write the stream header, if any. */
	if (avformat_write_header(ctx->outFormatCtx, NULL) < 0) {
	      printf("Error writing header to output file\n");
	      return 1;
	}

	// Dump information about file
	av_dump_format(ctx->inFormatCtx, 0, argv[1], 0);
	av_dump_format(ctx->outFormatCtx, 0, argv[2], 1);

	ctx->start_time = ctx->inVideoStream->start_time; 
//	ctx->configuration.expected_total_frames = ctx->configuration.avg_framerate * (((double)(pts[total_packets-1] - ctx->start_time + ctx->inVideoStream->start_time) / (double) (ctx->inVideoStream->start_time + ctx->inVideoStream->duration) * ctx->inFormatCtx->duration)/ AV_TIME_BASE);
	m_ptsOffset = av_rescale_q(ctx->start_time, ctx->inVideoStream->time_base, myAVTIMEBASEQ);
	printf("expected: %f duration: %" PRId64 " offset: %" PRId64 ", start %" PRId64 ", orig_start %" PRId64 "\n", ctx->configuration.expected_total_frames, ctx->inVideoStream->duration, m_ptsOffset, ctx->start_time, ctx->inVideoStream->start_time);

        pthread_create (&ctx->consumer, NULL, disk_writer, ctx);
	
	while(1) {
	   if(abort_requested)
		break;

	    av_init_packet(&ctx->inPacket);

	    if(av_read_frame(ctx->inFormatCtx, &ctx->inPacket) < 0) {
		printf("\nend of input stream\n");
		break;
	    }

	    if(ctx->inPacket.side_data_elems > 0) {
		printf("Sorry. Side data elements not supported in demuxed packets right now.\n");
		exit(1);
	    }

	    // Is this a packet from the video stream?
	    if(ctx->inPacket.stream_index==ctx->videoStream) {
			int speculative_frame_count = 1;
			queue_entry_t * e = malloc(ENTRYSIZE);
			Command * cmd = &e->cmd;
			assert(e);
			e->type = ctx->inPacket.stream_index;
			command__init(cmd);
			cmd->convert = &e->convert;
			convert__init(cmd->convert);
			cmd->convert->frame_number = ctx->configuration.frame_number;

			if(!ctx->ready_for_slaves) {
				if(!ctx->first_keyframe_reached && ctx->encoder_clean && ctx->drops_stopped) {
					if(ctx->inPacket.flags & AV_PKT_FLAG_KEY) {
						if(!ctx->remote)
							printf("\n");
						printf("First clean keyframe reached\n");
						ctx->first_keyframe_reached = 1;
					}
				}
				if(ctx->encoder_clean && ctx->drops_stopped && ctx->first_keyframe_reached) {
					if(!ctx->remote)
						printf("\n");
					printf("Ready for slaves.\n");
					ctx->ready_for_slaves = 1;
				}
			}

			while(ctx->remote && ctx->ready_for_slaves && ctx->num_slaves == 0 && !abort_requested) {
				printf("Waiting for slaves...\n");
				sleep(1);
			}

			if(ctx->remote && ctx->ready_for_slaves) {
				if(ctx->inPacket.flags & AV_PKT_FLAG_KEY)
					icount++;

				if(ctx->curr_slave == NULL) {
					ctx->curr_slave = &ctx->slaves[0];
					printf("\nSlave %d (%s) is ready for more work...\n", ctx->curr_slave->id, ctx->curr_slave->ip);
				} else {
					if(icount == RANGESIZE && ctx->inPacket.flags & AV_PKT_FLAG_KEY) {
						int p;
						icount = 0;
						slave_t * not_busy = NULL, * curr;
						do {
							int print = ((ctx->fifo->size % 5) == 0);
							//my_select(ctx, &socks, 0);
							
							for(p = 0; p < ctx->num_slaves; p++) {
								curr = &ctx->slaves[p];
								if(!curr->busy) {
									//if(!FD_ISSET(curr->fd, &socks)) {
									//	printf("\nSlave %d (%s) is ready buffer is FULLLLLLLLLLL...\n", p, curr->ip);
									//} else {
										printf("\nSlave %d (%s) is ready for more work...\n", p, curr->ip);
										not_busy = curr;
									//}
								}
								if(print)
								printf("Slave %d, queue %d, (%s) computed ranges %d frames remaining this range: %d\n",
									curr->id,
									curr->results->size,	
									curr->ip,
									curr->ranges,
									curr->busy);
							}

							if(not_busy) {
								not_busy->ranges++;
								ctx->curr_slave = not_busy;
								ctx->curr_slave->ranges++;
								ctx->ranges++;
								break;
							}

							if(print)
							printf("All slaves busy, queue %d\n", ctx->fifo->size);
							usleep(500000);
						} while(not_busy == NULL);
					}
				}
			}

			e->slave = ctx->curr_slave;
			e->m_pts = 0;
			e->current_frame_number = ctx->configuration.frame_number;

			if (ctx->inPacket.dts != AV_NOPTS_VALUE) {
				int64_t orig = ctx->inPacket.dts, rescaled;
				e->m_pts  = av_rescale_q(ctx->inPacket.dts, ctx->inVideoStream->time_base, myAVTIMEBASEQ);
				e->m_pts -= m_ptsOffset;
				if(debug==2)
				printf("orig: %" PRId64 " rescaled %" PRId64 " with offset %" PRId64 "\n", 
					orig, rescaled, e->m_pts);
			}

			e->inPts = (double)e->m_pts / AV_TIME_BASE;
			e->outFrames = e->inPts / av_q2d(ctx->outVideoCodecCtx->time_base);
			e->vdelta = e->outFrames - ctx->configuration.frame_number;

			if (e->vdelta < -1.1) {
				speculative_frame_count = 0;
			} else if (e->vdelta > 1.1) {
				speculative_frame_count = lrintf(e->vdelta);
			}

			cmd->convert->n_frame_count = speculative_frame_count; 

			if(ctx->remote && ctx->ready_for_slaves) {
				int released_packet = 0;
				cmd->convert->packet = &e->packet;
				packet__init(cmd->convert->packet);
				packet_to_protobuf(cmd->convert->packet, &ctx->inPacket);
				e->save_packet = ctx->inPacket;

				if(icount == (RANGESIZE - 1)) {
					if(gop_length == 0) {
						if(debug==2)
						printf("\nStart Ending GOP @ %" PRId64 "\n", ctx->inPacket.pts);
						assert(ctx->inPacket.flags & AV_PKT_FLAG_KEY);
					}
					if(gop_length == MAX_EXPECTED_GOP_SIZE) {
						printf("\nHuge GOP, man, more than %d\n", MAX_EXPECTED_GOP_SIZE);
						exit(1);
					}
					gop_entries[gop_length++] = *e;
					released_packet = 1;
				} else if(icount == 0) {
					if(ctx->inPacket.flags & AV_PKT_FLAG_KEY) {
						if(gop_length > 0) {
							printf("\nRetransmitting scene-change GOP @ %" PRId64 ", packets: %d, to slave: %d (%s)\n",
								gop_entries[0].cmd.convert->packet->pts, gop_length, e->slave->id, e->slave->ip);

							for(y = 0; y < gop_length; y++) {
								queue_entry_t * ge = &gop_entries[y];
								Command * gcmd = &ge->cmd;
								gcmd->code = TRANSCODE;
								gcmd->convert->do_not_reply = 1;
								got = cmdsend(e->slave->fd, e->slave->sbuf, gcmd);
								got = multiwrite(e->slave->fd, ge->save_packet.data, gcmd->convert->packet->size);
								if(got <= 0 || got != gcmd->convert->packet->size) {
									printf("gop wtf? %d / %d", got, gcmd->convert->packet->size);
									return -1;
								}
								
								av_free_packet(&ge->save_packet);
							}
							if(debug==2)
							printf("Retransmit complete\n");
							gop_length = 0;
						}
					}
				}	
				
				cmd->code = TRANSCODE;
				got = cmdsend(e->slave->fd, e->slave->sbuf, cmd);
				got = multiwrite(e->slave->fd, ctx->inPacket.data, ctx->inPacket.size);
				if(got <= 0 || got != ctx->inPacket.size) {
					printf("regular wtf? %d / %d", got, ctx->inPacket.size);
					return -1;
				}

				if(released_packet == 0)
					av_free_packet(&ctx->inPacket);
			} else {
				if(debug==2) printf("local transcode %" PRId64 "\n", ctx->inPacket.pts);
				transcode(ctx, e, cmd->convert, &ctx->inPacket);
				store_transcode_result(&e->cmd, e);
			}


			if(ctx->remote && ctx->ready_for_slaves) {
				if(ctx->curr_slave)
					ctx->curr_slave->busy++;
				push(ctx->fifo, e);
			} else {
				if(debug==2)
				printf("skipping producer, outputting now.\n");
				output_video(ctx, e, &e->cmd);
				/* 
				 * Don't farm packets until we've stopped dropping 
				 * them and the encoder is clean 
				 */
				if(!ctx->drops_stopped && cmd->convert->n_frame_count == 0) {
					printf("drops finished\n");
					ctx->drops_stopped = 1;
				}
				if(!ctx->encoder_clean && cmd->convert->extra_frame_count) {
					printf("encoder is clean\n");
					ctx->encoder_clean = 1; 
					ctx->configuration.frame_number += cmd->convert->extra_frame_count;
				}
				free(e);
				/* free local video transcode */
				av_free_packet(&ctx->inPacket);
			}
			ctx->configuration.frame_number += speculative_frame_count;
		} else if(ctx->inPacket.stream_index == ctx->audioStream) {
			queue_entry_t * e = malloc(ENTRYSIZE);
			assert(e);
			e->save_packet = ctx->inPacket;
			e->type = ctx->inPacket.stream_index;
			push(ctx->fifo, e);
		} else {
			/* free unknown packet */ 
			av_free_packet(&ctx->inPacket);
		}
		
	}
	printf("Stopping consumer.\n");
	consumer_stop(ctx);

	if(ctx->remote) {
		int idx;
		Command cmd;
		Convert convert;
		slave_t * slave;

		command__init(&cmd);
		convert__init(&convert);
		cmd.convert = &convert;

		ctx->stop_listener = 1;
		shutdown(ctx->sockd, SHUT_RDWR);
		printf("Waiting for listener to close...\n");
		pthread_join(ctx->listener, NULL);

		for(idx = 0; idx < ctx->num_slaves; idx++) {
			slave = &ctx->slaves[idx];
			printf("Closing slave %d, ranges %d\n", slave->id, slave->ranges);
			cmd.code = TRANSCODE;
			cmd.convert->finished = 1;
			got = cmdsend(slave->fd, slave->sbuf, &cmd);
			close(slave->fd);
		}
		close(ctx->sockd);
		printf("Total ranges: %d\n", ctx->ranges);

	}

	if(gop_length > 0) {
		printf("Freeing last unused gop\n");
		  for(y = 0; y < gop_length; y++)
			av_free_packet(&gop_entries[y].save_packet);
	}

	printf("(100 %%) Finished: frames: %d\n", ctx->configuration.frame_number);


	av_write_trailer(ctx->outFormatCtx);
	avio_close(ctx->outFormatCtx->pb);

	/* Free the streams. */
	for (x = 0; x < ctx->outFormatCtx->nb_streams; x++) {
		av_freep(&ctx->outFormatCtx->streams[x]->codec);
		av_freep(&ctx->outFormatCtx->streams[x]);
	}

	destroy_ctx_out(ctx); 
	queueDelete (ctx->fifo);
	destroy_ctx_in(ctx);
	avformat_close_input(&ctx->inFormatCtx);
	free(ctx);

	return 0;
}
