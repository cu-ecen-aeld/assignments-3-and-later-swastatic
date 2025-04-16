#include <arpa/inet.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>
#include <pthread.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <signal.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <syslog.h>

bool caught_sigint = false;
bool caught_sigterm = false;

static void signal_handler ( int signal_number )
{
	int errno_saved = errno;
	if ( signal_number == SIGINT ) {
		caught_sigint = true;
	} else if ( signal_number == SIGTERM ) {
		caught_sigterm = true;
	}
	errno = errno_saved;
}

struct thread_data{
	int sockfd;
	//char *buff;
	char buff[32];
	bool ret_stat;
};

void* threadfunc(void* thread_param)
{
	struct thread_data* thread_func_args = (struct thread_data *) thread_param;
	//thread_func_args->buff = (char *)malloc(64);
	struct sockaddr_in their_addr;

	//Listening and Accepting the connections
	if (listen(thread_func_args->sockfd, 1) == -1) {
		perror("listen");
		close(thread_func_args->sockfd);
		pthread_exit(NULL);
	}
	printf("Listening for connections...\n");
	socklen_t addrlen = sizeof(their_addr);

	while(1)
	{
		//Accept connections
		int new_fd = accept(thread_func_args->sockfd, (struct sockaddr *)&their_addr, &addrlen);
		if (new_fd == -1) {
			perror("accept");
			close(thread_func_args->sockfd);
			thread_func_args->ret_stat = false;
			exit(1);
		}
		
		char client_ip[16] = "";
		inet_ntop(AF_INET, &their_addr.sin_addr, client_ip, INET_ADDRSTRLEN);
		//printf("Accepted connection from %s\n", client_ip);
		syslog(LOG_INFO, "Accepted connection from %s\n", client_ip);

		//Open the file and write buff value into it
		int buff_fd = open("/var/tmp/aesdsocketdata", O_RDWR|O_CREAT|O_APPEND, S_IRWXU|S_IRWXG|S_IRWXO);

		int rd = recv(new_fd, thread_func_args->buff, sizeof(thread_func_args->buff), 0);
		while(rd > 0)
		{
			//Receiving data from the socket
			char *newline = strchr(thread_func_args->buff, '\n');
			if (newline) {
				size_t index = newline - thread_func_args->buff;
				thread_func_args->buff[index+1] = '\0';
			}
			unsigned char len = strlen(thread_func_args->buff);
			write(buff_fd, thread_func_args->buff, len);
			if (newline)
				break;
			memset(thread_func_args->buff, 0, sizeof(thread_func_args->buff));
			rd = recv(new_fd, thread_func_args->buff, sizeof(thread_func_args->buff), 0);
		}

		lseek(buff_fd, 0, SEEK_SET);
		ssize_t sent = read(buff_fd, thread_func_args->buff, sizeof(thread_func_args->buff));
		while(sent > 0)
		{
			send(new_fd, thread_func_args->buff, strlen(thread_func_args->buff), 0);
			memset(thread_func_args->buff, 0, sizeof(thread_func_args->buff));
			sent = read(buff_fd, thread_func_args->buff, sizeof(thread_func_args->buff));
		}
		//printf("Closed connection from %s\n", client_ip);
		syslog(LOG_INFO, "Closed connection from %s\n", client_ip);
		memset(thread_func_args->buff, 0, sizeof(thread_func_args->buff));
		close(buff_fd);
		close(new_fd);
	}
}

int main(int argc, char **argv)
{
	struct thread_data params;
	params.sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if(params.sockfd == -1)
	{
		perror("socket");
		return -1;
	}

	int optval = 1;
	if (setsockopt(params.sockfd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) == -1) {
		perror("setsockopt");
		close(params.sockfd);
	}

	//Setup the sockaddr	
	struct addrinfo hints;
	struct addrinfo *result;
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_flags = AI_PASSIVE;
	hints.ai_socktype = SOCK_STREAM;

	if(getaddrinfo(NULL, "9000", &hints, &result) != 0)
	{
		perror("getaddrinfo");
		close(params.sockfd);
		return -1;
	}

	//Bind the socket
	if(bind(params.sockfd, result->ai_addr, result->ai_addrlen) == -1)
	{
		perror("bind");
		close(params.sockfd);
		return -1;
	}
	freeaddrinfo(result);
	
	//Creating a fork process after successful bind
	pid_t chipid = fork();
	if(chipid == 0)
	{
		//Setting up the pthread for the while loop to read and send the content
		pthread_t thread;
		int rc = pthread_create(&thread, NULL, threadfunc, &params);
		if (rc != 0)
		{
			printf("Creation of pthread failed!\n");
			return false;
		}
		printf("Successfully created pthread!\n");

		//Setting up the signals
		struct sigaction new_action;
		bool success = true;
		memset(&new_action,0,sizeof(struct sigaction));
		new_action.sa_handler=signal_handler;
		if( sigaction(SIGTERM, &new_action, NULL) != 0 ) {
			printf("Error %d (%s) registering for SIGTERM",errno,strerror(errno));
			success = false;
		}
		if( sigaction(SIGINT, &new_action, NULL) ) {
			printf("Error %d (%s) registering for SIGINT",errno,strerror(errno));
			success = false;
		}

		if( success ) {
			printf("Waiting forever for a signal\n");
			pause();
			if( caught_sigint )
				printf("\nCaught SIGINT!\n");
			if( caught_sigterm )
				printf("\nCaught SIGTERM!\n");

			syslog(LOG_INFO, "Caught signal, exiting\n");
			close(params.sockfd);
			system("rm -rf /var/tmp/aesdsocketdata");
		}
	}
	/*else
	{
		if(waitpid(chipid, &status, 0) == -1)
			return -1;
		else if(WIFEXITED(status))
			return 0;
	}*/
}
