/* 
   NAME: Jorge Contreras
   EMAIL: jorgec9000@g.ucla.edu
   ID: 205379811
 */
#include <stdio.h>
#include <sys/types.h> 
#include <sys/wait.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <poll.h>
#include <signal.h>
#include <zlib.h>

int pid, newsockfd, time_to_exit = 1, client_or_shell;
int pipefd_stdin[2], pipefd_stdout[2];

void completion_status() {
  int status, exit_status = -1, exit_signal = -1;
  char write_string[50];
  bzero(write_string, 50);
  close(pipefd_stdin[1]);
  close(pipefd_stdout[0]);

  waitpid(pid, &status, 0);

  if(WIFEXITED(status))
    exit_status = WEXITSTATUS(status);
  if(WIFSIGNALED(status))
    exit_signal = WTERMSIG(status);

  sprintf(write_string, "SHELL EXIT SIGNAL=%d STATUS=%d\n", exit_signal, exit_status);
  fprintf(stderr, "%s", write_string);
  //strcpy(shell_exit, write_string);
  //  write(newsockfd, write_string, 50);
  time_to_exit = 0;
  return;
}

void sighandler(int signum) {
  fprintf(stderr, "Inside sighnadler");
  if(signum != 0 && client_or_shell) {
    completion_status();
    exit(0);
  }
  return;
}

void exit_handler(int exit_num, char exit_msg[], int fd_to_close[], int num_fds) {
  int i;

  if(exit_num != 0)
    fprintf(stderr, "%s: %s\n", exit_msg, strerror(errno));

  if(num_fds !=0)
    for(i = 0; i < num_fds; i = i + 1)
      close(fd_to_close[i]);

  exit(exit_num);
}

int main(int argc, char *argv[])
{
  int sockfd, portno, ret, i, compress_toggle = 0;
  socklen_t clilen;
  char buffer[256];
  char compress_buffer[256];
  int close_fds[5];
  struct sockaddr_in serv_addr, cli_addr;
  struct pollfd fds[3];
  int c = 1;
  int option_index = 0;
  int num_fds = 0;

  if (argc < 2)
    exit_handler(1, "Usage: ./lab1b-server --port=[1025 and up]", close_fds, num_fds);

  static struct option long_options[] =
    {
      {"port", required_argument, 0, 'p'},
      {"compress", no_argument, 0, 'c'},
      {0, 0, 0, 0}
    };

  while(c != -1) {
    c = getopt_long(argc, argv, "p:", long_options, &option_index);
    
    switch(c) {
    case 'p':
      portno = atoi(optarg);
      break;

    case 'c':
      compress_toggle = 1;
      break;

    case '?':
      exit_handler(1, "Usage: ./lab1b-server --port=[1025 and up]", close_fds, num_fds);
      break;
    }
  }

  sockfd = socket(AF_INET, SOCK_STREAM, 0);

  if (sockfd < 0) 
    exit_handler(1, "Socket error", close_fds, num_fds);

  close_fds[0] = sockfd;
  num_fds = 1;

  serv_addr.sin_family = AF_INET;
  serv_addr.sin_addr.s_addr = INADDR_ANY;
  serv_addr.sin_port = htons(portno);

  if (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) 
    exit_handler(1, "bind syscall error", close_fds, num_fds);

  listen(sockfd,5);
  clilen = sizeof(cli_addr);
  newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen);
  close(sockfd);
  close_fds[0] = newsockfd;

  if(pipe(pipefd_stdin) == -1)
    exit_handler(1, "Pipe error", close_fds, num_fds);

  close_fds[1] = pipefd_stdin[0];
  close_fds[2] = pipefd_stdin[1];
  num_fds = 3;

  if(pipe(pipefd_stdout) == -1)
    exit_handler(1, "Pipe error", close_fds, num_fds);

  close_fds[3] = pipefd_stdout[0];
  close_fds[4] = pipefd_stdout[1];
  num_fds = 5;

  fds[0].fd = newsockfd;
  fds[1].fd = pipefd_stdout[0];
  fds[0].events = POLLIN | POLLHUP | POLLERR;
  fds[1].events = POLLIN | POLLHUP | POLLERR;

  switch(pid = fork()) {
  case -1:
    exit_handler(1, "Fork error", close_fds, num_fds);
    break;
    
  case 0:     //child process
    close(pipefd_stdin[1]);
    close(pipefd_stdout[0]);
    close(newsockfd);
    
    close(0);
    dup(pipefd_stdin[0]);
    close(pipefd_stdin[0]);

    close(1);
    dup(pipefd_stdout[1]);
    close(2);
    dup(pipefd_stdout[1]);
    close(pipefd_stdout[1]);
    
    close_fds[0] = 0;
    close_fds[1] = 1;
    close_fds[2] = 2;
    num_fds = 3;
    char** empty_thing = NULL;

    if(execv("/bin/bash", empty_thing) == -1)
      exit_handler(1, "execv error", close_fds, num_fds);
    break;

  default:     //parent process
    close(pipefd_stdin[0]);
    close(pipefd_stdout[1]);
    signal(SIGPIPE, sighandler);

    close_fds[1] = pipefd_stdin[1];
    close_fds[2] = pipefd_stdout[0];
    num_fds = 3;
    
    z_stream deflate_stream, inflate_stream;
    deflate_stream.zalloc = Z_NULL;
    deflate_stream.zfree = Z_NULL;
    deflate_stream.opaque = Z_NULL;
    ret  = deflateInit(&deflate_stream, Z_DEFAULT_COMPRESSION);
    if(ret != Z_OK)
      exit_handler(1, "error with deflateInit", close_fds, num_fds);
    inflate_stream.zalloc = Z_NULL;
    inflate_stream.zfree = Z_NULL;
    inflate_stream.opaque = Z_NULL;
    ret = inflateInit(&inflate_stream);
    if(ret != Z_OK)
      exit_handler(1, "error with inflateInit", close_fds, num_fds);

    while(time_to_exit) {
      poll(fds, 2, 0);

      if(fds[0].revents & POLLIN) {     //input from client
	ret = read(fds[0].fd, buffer, 256);

	client_or_shell = 0;

	if(ret < 0) {
	  close(pipefd_stdin[1]);
	  close_fds[1] = close_fds[2];
	  num_fds = 2;
	  completion_status();
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
	  case 0x03:    //^C char
	    if(kill(pid, SIGINT) < 0)
	      exit_handler(1, "Error killing shell process", close_fds, num_fds);
	    break;

	  case 0x04:     //^D char, EOF
	    close(pipefd_stdin[1]);
	    close_fds[1] = close_fds[2];
	    num_fds = 2;
	    completion_status();
	    exit_handler(0, "", close_fds, num_fds);
	    break;

	  default:
	    write(pipefd_stdin[1], &buffer[i], 1); //work tbd	    
	    break;
	  }
	}
      }    
       
      if(fds[1].revents & POLLIN) {     //input from shell PIPE
	ret = read(fds[1].fd, buffer, 256);

	client_or_shell = 1;

	if(ret < 0)
	  exit_handler(1, "Server error reading from pipe to shell", close_fds, num_fds);
	  
	int complete_toggle = 0;
	for(i = 0; i < ret; i = i + 1)
	  if((int) buffer[i] == 0x04)
	    complete_toggle = 1;

	if(compress_toggle) {
	  bzero(compress_buffer, 256);
	  deflate_stream.avail_in = strlen(buffer);
	  deflate_stream.next_in = (Bytef *)buffer;
	  deflate_stream.avail_out = 256;
	  deflate_stream.next_out = (Bytef *) compress_buffer;
	  deflate(&deflate_stream, Z_SYNC_FLUSH);
	  ret = 256 - deflate_stream.avail_out;
	  bzero(buffer, 256);
	  for(i = 0; i < ret; i = i +1)
	    buffer[i] = compress_buffer[i];
	}

	write(newsockfd, buffer, ret);

	if(complete_toggle)
	  completion_status();
      }

      if((fds[1].revents & (POLLERR | POLLHUP)) || (fds[0].revents & (POLLHUP | POLLERR))) {
	completion_status();
      }
    }
  }

  num_fds = 1;
  exit_handler(0, "", close_fds, num_fds);
  
}
