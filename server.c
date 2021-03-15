#include <pthread.h> 
#include <unistd.h>
#include <stdio.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <string.h>
#include <stdint.h>
#include <time.h>

#define PORT        8080
#define BUFFER_SIZE 1024


char * message = "This is a message from the server. The server says howdy!";
pthread_t * user_clients; 
pthread_mutex_t lock;
char buffer[BUFFER_SIZE] = {0};


void * serve_client(void * arg) {
	char localbuffer[BUFFER_SIZE] = "u";
	char newmessage[strlen(message)+16];
	int socket = (int)(intptr_t)arg;
	while(localbuffer[0]==117 && strlen(localbuffer)==1) {
		pthread_mutex_lock(&lock);

    	nanosleep((const struct timespec[]){{5, 0}}, NULL);
		send(socket ,localbuffer ,strlen(localbuffer), 0);

    	pthread_mutex_unlock(&lock);
		nanosleep((const struct timespec[]){{1, 0}}, NULL);
		read(socket, localbuffer, BUFFER_SIZE);

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
	int client_count = 1;

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

		sockets = (int *)realloc(sockets,sizeof(int)*client_count);
		sockets[client_count-1] = new_socket;

		read(new_socket, buffer, BUFFER_SIZE);
		printf("%s\n",buffer);
		if(buffer[0]==117 && strlen(buffer)==1) {
			
			user_clients = (pthread_t * ) realloc(user_clients, sizeof(pthread_t)*client_count);
			error = pthread_create(&(user_clients[client_count-1]), 
								NULL, 
								&serve_client, (void *)(intptr_t)sockets[client_count-1]); 
			if (error != 0) 
				printf("\nThread can't be created :[%s]", 
					strerror(error));

			//pthread_join(user_clients[client_count-1], NULL);
			
		}
		printf("%d client(s) connected!\n",client_count);
		client_count++;
	}

    //send(new_socket ,resp ,strlen(resp), 0);
	//printf("Hello message sent\n");

    return 0;
}
