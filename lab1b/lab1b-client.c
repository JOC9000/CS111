/*
  NAME: Jorge Contreras
  EMAIL: jorgec9000@g.ucla.edu
  ID: 205379811
 */
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h> 
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <termios.h>
#include <poll.h>
#include <zlib.h>

void exit_handler(int exit_num, char error_info[], struct termios normal_set) {
  tcsetattr(0, TCSANOW, &normal_set);

  if(exit_num != 0)
    fprintf(stderr, "%s: %s\n", error_info, strerror(errno));

  exit(exit_num);
} 

int main(int argc, char *argv[])
{
  int sockfd, portno, ret, i, log_fd, compress_toggle = 0;
  struct sockaddr_in serv_addr;
  struct hostent *server;
  struct termios to_change, saved_settings;
  struct pollfd fds[2];
  char buffer[256];
  char sr_buffer[300];
  char true_buffer[256];
  char compress_buffer[256];
  int c = 1;
  int option_index = 0;
  int time_to_exit = 1;
  int log_toggle = 0;

  tcgetattr(0, &to_change);
  saved_settings = to_change;

  to_change.c_iflag = ISTRIP;
  to_change.c_oflag = 0;
  to_change.c_lflag = 0;

  if (argc < 2) {
    fprintf(stderr, "Usage: ./lab1b-client --port=[1025 or up] [--log=filename] [--compress]");
    exit(1);
  }

  static struct option long_options[] =
    {
      {"port", required_argument, 0, 'p'},
      {"log", required_argument, 0, 'l'},
      {"compress", no_argument, 0, 'c'},
      {0, 0, 0, 0}
    };

  while(c != -1) {
    c = getopt_long(argc, argv, "p:", long_options, &option_index);
    
    switch(c) {
    case 'p':
      portno = atoi(optarg); 
      break;

    case 'l':
      log_fd = creat(optarg, S_IRWXU);
      if(log_fd < 0) {
	fprintf(stderr, "Error creating log file");
	exit(1);
      }
      log_toggle = 1;
      break;

    case 'c':
      compress_toggle = 1;
      break;

    case '?':
      fprintf(stderr, "Usage: ./lab1b-client --port=[1025 or up] [--log=filename] [--compress]");
      exit(1); 
      break;
    }
  }

  tcsetattr(0, TCSANOW, &to_change);

  sockfd = socket(AF_INET, SOCK_STREAM, 0);

  if (sockfd < 0)
    exit_handler(1, "Error creating socket", saved_settings);

  server = gethostbyname("localhost");

  if (server == NULL)
    exit_handler(1, "ERROR, no such host", saved_settings);

  serv_addr.sin_family = AF_INET;
  bcopy((char*)server->h_addr, (char*)&serv_addr.sin_addr.s_addr, server->h_length);
  serv_addr.sin_port = htons(portno);

  if (connect(sockfd,(struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
    exit_handler(1, "ERROR connecting", saved_settings);

  fds[0].fd = 0;
  fds[1].fd = sockfd;
  fds[0].events = POLLIN | POLLHUP | POLLERR;
  fds[1].events = POLLIN | POLLHUP | POLLERR;

  z_stream deflate_stream, inflate_stream;
  deflate_stream.zalloc = Z_NULL;
  deflate_stream.zfree = Z_NULL;
  deflate_stream.opaque = Z_NULL;
  ret = deflateInit(&deflate_stream, Z_DEFAULT_COMPRESSION);
  if(ret != Z_OK)
    exit_handler(1, "error with deflateInit", saved_settings);
  inflate_stream.zalloc = Z_NULL;
  inflate_stream.zfree = Z_NULL;
  inflate_stream.opaque = Z_NULL;
  ret = inflateInit(&inflate_stream);
  if(ret != Z_OK)
    exit_handler(1, "error with inflateInit", saved_settings);

  while(time_to_exit) {
    if(poll(fds, 2, 0) < 0)
      exit_handler(1, "Poll error", saved_settings);

    if(fds[0].revents & POLLIN) {    //stdin processing
      bzero(true_buffer, 256);
      ret = read(0, buffer, 256);

      if(ret < 0)
	exit_handler(1, "Read error", saved_settings);

      for(i = 0; i < ret; i = i + 1) {
	switch((int) buffer[i]) {
	case 0x0D:    //\r char, charraige return
	case 0x0A:     //\n char, linefeed
	  fprintf(stdout, "\r\n");
	  true_buffer[i] = '\n';
	  break;
	  
	default:
	  true_buffer[i] = buffer[i];

	  if(write(1, &buffer[i], 1) < 0) 
	    exit_handler(1, "Client writer error to stdout", saved_settings);
	  break;
	}
      }

      if(compress_toggle) {
	bzero(compress_buffer, 256);
	deflate_stream.avail_in = strlen(true_buffer);
	deflate_stream.next_in = (Bytef *) true_buffer;
	deflate_stream.avail_out = 256;
	deflate_stream.next_out = (Bytef *) compress_buffer;
	deflate(&deflate_stream, Z_SYNC_FLUSH);
	ret = 256 - deflate_stream.avail_out;
	bzero(true_buffer, 256);
	for(i = 0; i < ret; i = i + 1)
	  true_buffer[i] = compress_buffer[i];
      }

      if(write(sockfd, &true_buffer, ret) < 0)
	exit_handler(1, "Client write to server error", saved_settings);
	
      if(log_toggle) {
	sprintf(sr_buffer, "SENT %d bytes: %s\n", ret, true_buffer);
	write(log_fd, sr_buffer, strlen(sr_buffer));
      }
    }

    if(fds[1].revents & POLLIN) {     //read input from server
      ret = read(sockfd, buffer, 256);

      if(ret < 0)
	exit_handler(1, "Read error", saved_settings);

      if(ret == 0)
	time_to_exit = 0;

      if(log_toggle && ret > 0) {
	sprintf(sr_buffer, "RECEIVED %d bytes: %s\n", ret, buffer);
	write(log_fd, sr_buffer, strlen(sr_buffer));
      }

      if(compress_toggle) {
	bzero(compress_buffer, 256);
	inflate_stream.avail_in = ret;
	inflate_stream.next_in = (Bytef *)buffer;
	inflate_stream.avail_out = 256;
	inflate_stream.next_out = (Bytef *)compress_buffer;
	inflate(&inflate_stream, Z_SYNC_FLUSH);
	bzero(buffer, 256);
	ret = strlen(compress_buffer);
	for(i = 0; i < ret; i = i + 1)
	  buffer[i] = compress_buffer[i];
      }

      for(i = 0; i < ret; i = i + 1) {
	switch((int) buffer[i]) {
	case 0x0A:    //\n char, linefeed
	  fprintf(stdout, "\r\n");
	  break;

	default:
	  if(write(1, &buffer[i], 1) < 0)
	    exit_handler(1, "Write error", saved_settings);
	break;
	}
      }
    }

    if(fds[1].revents & (POLLHUP | POLLERR))
      time_to_exit = 0;

  }

  inflateEnd(&inflate_stream);
  deflateEnd(&deflate_stream);
  close(sockfd);
  exit_handler(0, "", saved_settings);
}
