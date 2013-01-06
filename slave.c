#include "luvi.h"

static void * video_transcoder(void * opaque)
{
	Context 	* ctx = opaque;
	queue_entry_t   * e;

	while(1) {
		if(!(e = pop(ctx->in_fifo, &ctx->stop_in_consumer)))
			break;

		// transcoding parameters for this frame
		protobuf_to_packet(&e->save_packet, e->recv->convert->packet);

		// go!
		e->got_packet = transcode(ctx, e, e->recv->convert, &e->save_packet);
		update_fps(ctx, 0);

		/* 
		 * Free the original data. Resulting coded frame is
		 * stored in e->outBuffers
		 */
		av_free_packet(&e->save_packet);

		push(ctx->out_fifo, e);
        }
}

static void * network_writer(void * opaque)
{
	Context 	* ctx = opaque;
	queue_entry_t   * e;
	Command 	send;
	Convert 	convert;
	uint8_t		buf[MAX_MSG_SIZE];
	int		x,
			ret;


	while(1) {
		if(!(e = pop(ctx->out_fifo, &ctx->stop_out_consumer)))
			break;

		/* 
		 * Packet data has been previous allocated.
		 * Prepare response area.
		 */
		command__init(&send);	
		send.convert = &convert;
		convert__init(&convert);

		// prepare response to master
		send.convert->buffers = e->recv->convert->buffers;
		send.convert->extra_frame_count = e->recv->convert->extra_frame_count;
		send.convert->frame_number = e->recv->convert->frame_number;
		send.convert->n_frame_count = e->recv->convert->n_frame_count;

		store_transcode_result(&send, e);

		// if transcoding produced a frame, send the corresponding buffers
		if(e->recv->convert->do_not_reply == 0) {
			// Tell master what this is
			send.code = TRANSCODE_RESULT;
			cmdsend(e->fd, buf, &send);

			if(e->got_packet) {
				/* Send raw data */
				for(x = 0; x < send.convert->buffers; x++) {
					ret = multiwrite(e->fd, e->outBuffers[x], e->buffer_lengths[x]);	
					if(ret <= 0) {
						perror("write");
						printf("Failed to send output packet payloads.\n");
						exit(1);
					}
				}
			}
		}

		for(x = 0; x < send.convert->buffers; x++)
			av_free(e->outBuffers[x]);

		command__free_unpacked(e->recv, NULL);
		free(e);
        }
}

int main(int argc, char * argv[]) {
	Command 	       * cmd, 
			  	send;
        struct addrinfo        * raddr;
	struct sockaddr_in      saddr;
        int 			rport, 
				fd, 
				ret,
				rcvbuff = NETWORK_BUFFER, 
				sendbuff = NETWORK_BUFFER, 
				res;
	char 			rip[20];
        pthread_t 		pro;
	uint8_t			buf[MAX_MSG_SIZE];
	Context               * ctx;

	ctx = malloc(sizeof(Context));

	if(init_ctx(ctx, QUEUESIZE) < 0)
		return -1;

        if(argc < 3) {
            printf("Usage: <master address / ip> <port>\n");
            exit(1);
        }

	raddr = gethost(argv[1]);
        strcpy(rip, inet_ntoa(((struct sockaddr_in *) raddr->ai_addr)->sin_addr));
	printf("%s mapped to %s\n", argv[1], rip);

        rport = atoi(argv[2]);

	if ((fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0) {
		perror("");
		exit(1);
	}

	bzero(&saddr, sizeof(saddr));
        saddr.sin_family      = AF_INET;
        saddr.sin_addr.s_addr = inet_addr(rip);
        saddr.sin_port        = htons(rport); /* daytime server */

	if(connect(fd, (struct sockaddr *) &saddr, sizeof(struct sockaddr_in)) < 0) {
		perror("connect");
		exit(1);
	} 

	res = setsockopt(fd, SOL_SOCKET, SO_RCVBUF, &rcvbuff, sizeof(rcvbuff));	

	if(res < 0) {
		perror("setsockopt: receive buffer:");
		printf("Failed to increase new slave's receive buffer\n");
		exit(1);
	}

	res = setsockopt(fd, SOL_SOCKET, SO_SNDBUF, &sendbuff, sizeof(sendbuff));	

	if(res < 0) {
		perror("setsockopt: send buffer:");
		printf("Failed to increase new slave's send buffer\n");
		exit(1);
	}

	printf("== connected %d with buffer %d %d\n", fd, sendbuff, rcvbuff);

	// Announce ourselves to the master and ask for work to do
	command__init(&send);
	send.code = INIT;
	send.name = ctx->hostname;
	ret = cmdsend(fd, buf, &send);
	ret = cmdrecv(fd, buf, &cmd, CODEC);
	memcpy(&ctx->configuration, cmd->configuration, sizeof(Config)); 
	command__free_unpacked(cmd, NULL);

	if(init_in(ctx, 1) < 0)
		return -1;

	if(init_out(ctx, 1) < 0)
		return -1;

        pthread_create (&ctx->in_consumer, NULL, video_transcoder, ctx);
        pthread_create (&ctx->out_consumer, NULL, network_writer, ctx);

	update_fps(ctx, 1);

	// Run until there is no more work to do
	while(1) {
		queue_entry_t * e = malloc(ENTRYSIZE);
		int x, size;
		void * destruct, * data;

		/* 
		 * Receive parameters to transcode one frame.
		 * An entire GOP will follow.
	         */
		cmdrecv(fd, buf, &e->recv, TRANSCODE);

		if(e->recv->convert->finished) {
			printf("\nEncoding complete.\n");
			break;
		}

		/*
		 * Pull in the raw packet data corresponding to this frame
		 * for transcoding.
		 */
		size = e->recv->convert->packet->size;

		// Allocate space to receive the data
		av_new_packet(&e->save_packet, size); 
		e->fd = fd;
		multirecv(fd, e->save_packet.data, size);

		// Send to FIFO for transcoding
		push(ctx->in_fifo, e);
	}

	// Wait for remaining transcodes to complete
	in_consumer_stop(ctx);
	out_consumer_stop(ctx);
	printf("Finished!\n");
	close(fd);
	destroy_ctx(ctx);
        freeaddrinfo(raddr);
}
