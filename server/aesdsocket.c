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
	char buff[1024];
	char client_ip[16];
	int new_fd;
	pthread_t thread;
};

struct node{
	struct thread_data data;
	struct node *next;
};

struct node * create_node()
{
	struct node * new_node = (struct node *)malloc(sizeof(struct node));
	new_node->next = NULL;
	return new_node;
}

pthread_mutex_t mutex;
void* threadfunc(void* thread_param)
{
	pthread_mutex_lock(&mutex);
	struct thread_data* thread_func_args = (struct thread_data *) thread_param;

	//Open the file and write buff value into it
	int buff_fd = open("/var/tmp/aesdsocketdata", O_RDWR|O_CREAT|O_APPEND, S_IRWXU|S_IRWXG|S_IRWXO);

	int rd = recv(thread_func_args->new_fd, thread_func_args->buff, sizeof(thread_func_args->buff)-1, 0);
	thread_func_args->buff[rd] = '\0';
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
		rd = recv(thread_func_args->new_fd, thread_func_args->buff, sizeof(thread_func_args->buff)-1, 0);
		thread_func_args->buff[rd] = '\0';
	}

	lseek(buff_fd, 0, SEEK_SET);
	ssize_t sent = read(buff_fd, thread_func_args->buff, sizeof(thread_func_args->buff)-1);
	thread_func_args->buff[sent] = '\0';
	while(sent > 0)
	{
		send(thread_func_args->new_fd, thread_func_args->buff, strlen(thread_func_args->buff), 0);
		memset(thread_func_args->buff, 0, sizeof(thread_func_args->buff));
		sent = read(buff_fd, thread_func_args->buff, sizeof(thread_func_args->buff)-1);
		thread_func_args->buff[sent] = '\0';
	}
	pthread_mutex_unlock(&mutex);
	syslog(LOG_INFO, "Closed connection from %s\n", thread_func_args->client_ip);
	memset(thread_func_args->buff, 0, sizeof(thread_func_args->buff));
	close(buff_fd);
	close(thread_func_args->new_fd);
	pthread_exit(&thread_func_args->thread);
	return NULL;
}

void* time_writer(void* thread_param)
{
	char timestr[200];
	time_t t;
	struct tm *tmp;

	while(1)
	{
		sleep(10);
		t = time(NULL);
		tmp = localtime(&t);
		if (tmp == NULL) {
			perror("localtime");
			exit(EXIT_FAILURE);
		}

		if (strftime(timestr, sizeof(timestr), "%Y-%m-%d %H:%M:%S", tmp) == 0) {
			fprintf(stderr, "strftime returned 0");
			exit(EXIT_FAILURE);
		}

		pthread_mutex_lock(&mutex);
		int time_fd = open("/var/tmp/aesdsocketdata", O_RDWR|O_CREAT|O_APPEND, S_IRWXU|S_IRWXG|S_IRWXO);
		write(time_fd, "timestamp:", 10);
		write(time_fd, timestr, strlen(timestr));
		write(time_fd, "\n", 1);
		close(time_fd);
		pthread_mutex_unlock(&mutex);
	}
	return NULL;
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
		//Listening and Accepting the connections
		if (listen(params.sockfd, 1) == -1) {
			perror("listen");
			close(params.sockfd);
			pthread_exit(NULL);
		}
		printf("Listening for connections...\n");

		if (pthread_mutex_init(&mutex, NULL) != 0) {
        		perror("mutex initialization failed");
        		exit(EXIT_FAILURE);
		}

		pthread_t timethread;
		struct node * head = NULL;
		struct node * last = NULL;

		int rc = pthread_create(&timethread, NULL, time_writer, NULL);
		if (rc != 0)
		{
			printf("Creation of pthread failed!\n");
			return false;
		}

		//Setting up the signals
		struct sigaction new_action;
		memset(&new_action,0,sizeof(struct sigaction));
		new_action.sa_handler=signal_handler;
		if( sigaction(SIGTERM, &new_action, NULL) != 0 ) {
			printf("Error %d (%s) registering for SIGTERM",errno,strerror(errno));
		}
		if( sigaction(SIGINT, &new_action, NULL) ) {
			printf("Error %d (%s) registering for SIGINT",errno,strerror(errno));
		}

		while(!caught_sigint && !caught_sigterm)
		{
			socklen_t addrlen = sizeof(struct sockaddr_in);
			//Accept connections
			struct sockaddr_in their_addr;
			params.new_fd = accept(params.sockfd, (struct sockaddr *)&their_addr, &addrlen);
			if (params.new_fd == -1) {
				perror("accept");
				if (errno == EINTR) {
					break;
				}
				continue;
			}
			inet_ntop(AF_INET, &their_addr.sin_addr, params.client_ip, INET_ADDRSTRLEN);
			syslog(LOG_INFO, "Accepted connection from %s\n", params.client_ip);

			struct node *thread_node = create_node();
			thread_node->data.sockfd = params.sockfd;
        		strcpy(thread_node->data.client_ip, params.client_ip);
			thread_node->data.new_fd = params.new_fd;
			if(last == NULL)
			{
				head = thread_node;
				last = head;
			}
			else
			{
				last->next = thread_node;
				last = thread_node;
			}

			//Setting up the pthread for the while loop to read and send the content
			rc = pthread_create(&(thread_node->data.thread), NULL, threadfunc, &(thread_node->data));
			if (rc != 0)
			{
				printf("Creation of pthread failed!\n");
				struct node *temp = head;
				while (temp != NULL) {
					close(temp->data.sockfd);
					close(temp->data.new_fd);
					pthread_join(temp->data.thread, NULL);
					struct node *to_free = temp;
					temp = temp->next;
					free(to_free);
				}
				return false;
			}
		}

		if( caught_sigint )
			printf("\nCaught SIGINT!\n");
		if( caught_sigterm )
			printf("\nCaught SIGTERM!\n");

		syslog(LOG_INFO, "Caught signal, exiting\n");
		pthread_mutex_destroy(&mutex);
		system("rm -rf /var/tmp/aesdsocketdata");
		//pthread_join(timethread, NULL);
		struct node *temp = head;
		while (temp != NULL) {
			close(temp->data.sockfd);
			close(temp->data.new_fd);
			pthread_join(temp->data.thread, NULL);
			struct node *to_free = temp;
			temp = temp->next;
			free(to_free);
		}
	}
	else
		exit(EXIT_SUCCESS);
}
