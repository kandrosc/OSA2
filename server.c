#include <pthread.h> 
#include <unistd.h>
#include <stdio.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <string.h>
#include <stdint.h>
#include <limits.h>
#include <time.h>
#include "notapp.h"

#define PORT        8080
#define BUFFER_SIZE 1024
#define EVENT_STRUCT_SIZE sizeof(struct eventinfo) 
#define EVENT_BUFFER_SIZE (10 * (EVENT_STRUCT_SIZE + NAME_MAX + 1))


char * message = "This is a message from the server. The server says howdy!";
pthread_t * user_clients; 
pthread_t * obs_clients;
pthread_mutex_t lock;
char buffer[BUFFER_SIZE] = {0};
struct eventinfo * critical_event_data; // This is the critical data structure that holds the most recent event data for each observer!
int critical_event_len = 0;


void * serve_client(void * arg) {
	char localbuffer[BUFFER_SIZE] = "u"; // If user client sends the letter "u", it is still alive, and awaiting updates
	char event[FIELDLEN*4+4];
	char * updates;
	struct process_args pargs = *(struct process_args *)arg;
	int socket = pargs.socket;
	int seconds = (int)pargs.interval;
	void * critical_event_len_ptr;
	critical_event_len_ptr = &critical_event_len;
	updates = (char *)malloc(FIELDLEN*4+4);
	while(localbuffer[0]==117 && strlen(localbuffer)==1) {
		strcpy(updates,"");
    	nanosleep((const struct timespec[]){{seconds, (pargs.interval-seconds)*1000000000L}}, NULL);
		pthread_mutex_lock(&lock);
		send(socket,critical_event_len_ptr,sizeof(int),0);
		if(critical_event_len>0) {
			updates = (char *)realloc(updates,(FIELDLEN*4+3)*critical_event_len);
			for(int i=0;i<critical_event_len;i++) {
				snprintf(event,FIELDLEN*4+4,"%s\t%s\t%s\t%s\n",critical_event_data[i].timestamp,critical_event_data[i].host,critical_event_data[i].monitored,critical_event_data[i].event);
				strcat(updates,event);
			}
		}
		send(socket ,updates ,FIELDLEN*4+4, 0);
    	pthread_mutex_unlock(&lock);
		nanosleep((const struct timespec[]){{0, 100000000L}}, NULL);
		read(socket, localbuffer, BUFFER_SIZE);

	}
    return NULL; 
}

void * receive_events(void * arg) {
	struct eventinfo info;
	void * info_ptr;
	struct process_args pargs = *(struct process_args *)arg;
	int socket = pargs.socket;
	int proc_id = pargs.proc_id;
	info.terminate = 0;
	info_ptr = (void *)malloc(sizeof(struct eventinfo));
	// Add new structure for the new observer client
	pthread_mutex_lock(&lock);
	critical_event_data = (struct eventinfo *)realloc(critical_event_data, sizeof(struct eventinfo)*proc_id);
    pthread_mutex_unlock(&lock);
	while(info.terminate == 0) {
		read(socket,info_ptr,sizeof(info));
		info = *(struct eventinfo *)info_ptr;
		// Write updated info to critical data structure
		pthread_mutex_lock(&lock);
		critical_event_data[proc_id-1] = info;
		critical_event_len = proc_id;
    	pthread_mutex_unlock(&lock);
		//printf("%s\t%s\t%s\t%s\n",critical_event_data[proc_id-1].timestamp,critical_event_data[proc_id-1].host,critical_event_data[proc_id-1].monitored,critical_event_data[proc_id-1].event);

	}
	return NULL;
}

int main(int argc, char const *argv[]) {
	int server_fd, new_socket;
	int * sockets;
	struct sockaddr_in address;
	int opt = 1;
	int addrlen = sizeof(address);
	int error;
	int user_client_count = 1;
	int obs_client_count = 1;

	struct process_args pargs;
	void * pargs_ptr;

	pargs_ptr = & pargs;
	pargs.interval = 5;

	if (pthread_mutex_init(&lock, NULL) != 0) { 
        printf("\n mutex init has failed\n"); 
        return 1; 
    } 

	user_clients = (pthread_t * )malloc(sizeof(pthread_t));
	sockets = (int *)malloc(sizeof(int));

	if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
		perror("socket failed");
		exit(EXIT_FAILURE);
	}

	address.sin_family = AF_INET;
	address.sin_addr.s_addr = INADDR_ANY;
	address.sin_port = htons(PORT);

	if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
		perror("bind failed");
		exit(EXIT_FAILURE);
	}

    if (listen(server_fd, 3) < 0) {
		perror("listen");
		exit(EXIT_FAILURE);
	}


	while(1) {

		if ((new_socket = accept(server_fd, (struct sockaddr *)&address,
						(socklen_t*)&addrlen))<0) {
			perror("accept");
			exit(EXIT_FAILURE);
		}

		sockets = (int *)realloc(sockets,sizeof(int)*user_client_count);
		sockets[user_client_count-1] = new_socket;
		pargs.socket = new_socket;

		read(new_socket, buffer, BUFFER_SIZE);
		printf("%s\n",buffer);
		if(buffer[0]==117 && strlen(buffer)==1) {
			pargs.proc_id = user_client_count;
			user_clients = (pthread_t * ) realloc(user_clients, sizeof(pthread_t)*user_client_count);
			error = pthread_create(&(user_clients[user_client_count-1]), 
								NULL, 
								&serve_client, pargs_ptr); 
			if (error != 0) 
				printf("\nThread can't be created :[%s]", 
					strerror(error));

			printf("%d user client(s) connected!\n",user_client_count);
			user_client_count++;
		}

		else if(buffer[0]==111 && strlen(buffer)==1) {
			pargs.proc_id = obs_client_count;
			obs_clients = (pthread_t * ) realloc(obs_clients, sizeof(pthread_t)*obs_client_count);
			error = pthread_create(&(obs_clients[obs_client_count-1]), 
								NULL, 
								&receive_events, pargs_ptr); 
			if (error != 0) 
				printf("\nThread can't be created :[%s]", 
					strerror(error));

			printf("%d observer client(s) connected!\n",obs_client_count);
			obs_client_count++;
		}

	}

    //send(new_socket ,resp ,strlen(resp), 0);
	//printf("Hello message sent\n");

    return 0;
}
