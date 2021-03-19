#include <pthread.h> 
#include <sys/inotify.h>
#include <unistd.h>
#include <stdio.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <string.h>
#include <stdint.h>
#include <limits.h>
#include <sys/time.h>
#include <setjmp.h>
#include <signal.h>
#include "notapp.h"

#define BUFFER_SIZE 1024
#define EVENT_BUFFER_SIZE (10 * (EVENT_STRUCT_SIZE + NAME_MAX + 1))
#define EVENT_STRUCT_SIZE sizeof(struct inotify_event) 
#define EVENT_BUFFER_SIZE (10 * (EVENT_STRUCT_SIZE + NAME_MAX + 1))
#define EVENTS IN_CREATE|IN_DELETE|IN_ATTRIB|IN_OPEN|IN_ACCESS|IN_MODIFY|IN_CLOSE_WRITE|IN_CLOSE_NOWRITE|IN_MOVED_FROM|IN_MOVED_TO|IN_DELETE_SELF|IN_IGNORED
sigjmp_buf state;


char * message = "This is a message from the server. The server says howdy!";
pthread_t * user_clients; 
pthread_t * obs_clients;
pthread_mutex_t lock;
char buffer[BUFFER_SIZE] = {0};
struct eventinfo * critical_event_data; // This is the critical data structure that holds the most recent event data for each observer!
int critical_event_len = 0;

// Call when CTRL-C is pressed
void catch() {
    siglongjmp(state,1);
}

void * serve_client(void * arg) {
	char localbuffer[BUFFER_SIZE] = "u"; // If user client sends the letter "u", it is still alive, and awaiting updates
	char event[FIELDLEN*4+4];
	char * updates;
	struct process_args pargs = *(struct process_args *)arg;
	int socket = pargs.socket;
	int seconds = (int)pargs.interval;
	void * critical_event_len_ptr;
	char * template_event = "%s\t%s\t%s\t%s\n";
	char * template_no_event = "%s";
	critical_event_len_ptr = &critical_event_len;
	updates = (char *)malloc(FIELDLEN*4+4);
	while(localbuffer[0]==117 && strlen(localbuffer)==1) {
		strcpy(updates,"");
    	nanosleep((const struct timespec[]){{seconds, (pargs.interval-seconds)*1000000000L}}, NULL);
		pthread_mutex_lock(&lock);
		send(socket,critical_event_len_ptr,sizeof(int),0);
		if(critical_event_len>0) {
			updates = (char *)realloc(updates,(FIELDLEN*4+4)*critical_event_len);
			for(int i=0;i<critical_event_len;i++) {
				if(strlen(critical_event_data[i].event) == 0) {
					snprintf(event,FIELDLEN*4+4,template_no_event,"");
				}
				else{
					snprintf(event,FIELDLEN*4+4,template_event,critical_event_data[i].timestamp,critical_event_data[i].host,critical_event_data[i].monitored,critical_event_data[i].event);
				}
				strcat(updates,event);
			}
		}
		send(socket ,updates ,(FIELDLEN*4+4)*critical_event_len, 0);
    	pthread_mutex_unlock(&lock);
		nanosleep((const struct timespec[]){{0, 100000000L}}, NULL);
		read(socket, localbuffer, BUFFER_SIZE);

	}
    return NULL; 
}

void * receive_events(void * arg) {
	struct eventinfo info;
	struct eventinfo emptyinfo;
	void * info_ptr;
	struct process_args pargs = *(struct process_args *)arg;
	int socket = pargs.socket;
	int proc_id = pargs.proc_id;
	info.terminate = 0;
	info_ptr = (void *)malloc(sizeof(struct eventinfo));
	int is_alive = 0;
	int * is_alive_ptr;
	is_alive_ptr = &is_alive;
	// Add new structure for the new observer client
	pthread_mutex_lock(&lock);
	critical_event_data = (struct eventinfo *)realloc(critical_event_data, sizeof(struct eventinfo)*proc_id);
    pthread_mutex_unlock(&lock);
	while(info.terminate == 0) {
		send(socket,is_alive_ptr,sizeof(int),0);
		read(socket,info_ptr,sizeof(info));
		info = *(struct eventinfo *)info_ptr;
		// Write updated info to critical data structure
		pthread_mutex_lock(&lock);
		critical_event_data[proc_id-1] = info;
		//critical_event_len = proc_id; // Must fix so out of order updates are displayed correctly
    	pthread_mutex_unlock(&lock);
		//printf("%s\t%s\t%s\t%s\n",critical_event_data[proc_id-1].timestamp,critical_event_data[proc_id-1].host,critical_event_data[proc_id-1].monitored,critical_event_data[proc_id-1].event);

	}
	is_alive = -1;
	send(socket,is_alive_ptr,sizeof(int),0);
	pthread_mutex_lock(&lock);
	critical_event_data[proc_id-1] = emptyinfo;
	pthread_mutex_unlock(&lock);
	return NULL;
}

int check_name(char * name) {
    for(int i=0;i<strlen(name);i++) {
        if(name[i] < 32 || name[i] > 127) {return 0;}
    }
    return 1;
}

// Checks to see if server is sending a signal that will tell send events to stay alive
void * check_is_alive(void *arg) {
    int socket = (int)(intptr_t)arg;
    int is_alive=0;
    int * is_alive_ptr;
    is_alive_ptr = &is_alive;

    while(is_alive == 0) {
        read(socket,is_alive_ptr,sizeof(int));
        is_alive = *(int *)is_alive_ptr;
    }
}

void * send_events(void * arg) {
	struct client_args cargs = *(struct client_args *)arg;
    int socket = cargs.port;
    struct eventinfo info;
    void * info_ptr;
    char * filename;
    char * name;
    struct timeval current_time;



    // create a new inotify file descriptor
    // it has an associated watch list
    // read from it to get events
    info_ptr = &info;
    
    int inotify_fd = inotify_init();
    if(inotify_fd < 0) {
        perror ("inotify_init");
        return NULL; // can't create the inotify fd, return 1 to os and exit
    }
    filename = cargs.filename;
    // add a new watch to inotify_fd, monitor the current folder for file/directory creation and deletion
    // returns a watch descriptor. 
    int watch_des = inotify_add_watch(inotify_fd, filename,EVENTS);
    if(watch_des == -1) {
        perror ("inotify_add_watch");
        return NULL; // can't create the watch descriptor, return 1 to os and exit
    }

    snprintf(info.monitored,FIELDLEN,"%s",filename);
    snprintf(info.host,FIELDLEN,"%s",cargs.address);
    info.terminate = 0;

    char buffer[EVENT_BUFFER_SIZE];

    // start to monitor
    while(info.terminate == 0) {
        // read 
        int bytesRead = read(inotify_fd, buffer, EVENT_BUFFER_SIZE), bytesProcessed = 0;
        if(bytesRead < 0) { // read error
            perror("read");
            return NULL;
        }

        while(bytesProcessed < bytesRead) {
            struct inotify_event* event = (struct inotify_event*)(buffer + bytesProcessed);
            if(check_name(event->name) == 1) {name = event->name;}
            else {name = filename;}
            if (event->mask & IN_CREATE)
                if (event->mask & IN_ISDIR)
                    snprintf(info.event,FIELDLEN,"%s IN_CREATE IN_ISDIR", name);
                else
                    snprintf(info.event,FIELDLEN,"%s IN_CREATE", name);
            else if ((event->mask & IN_DELETE))
                if (event->mask & IN_ISDIR)
                    snprintf(info.event,FIELDLEN,"%s IN_DELETE IN_ISDIR", name);
                else
                    snprintf(info.event,FIELDLEN,"%s IN_DELETE", name);
            else if ((event->mask & IN_ATTRIB))
                if(event->mask & IN_ISDIR)
                    snprintf(info.event,FIELDLEN,"%s IN_ATTRIB IN_ISDIR\n",name);
                else
                    snprintf(info.event,FIELDLEN,"%s IN_ATTRIB\n",name);
            else if ((event->mask & IN_OPEN))
                if(event->mask & IN_ISDIR)
                    snprintf(info.event,FIELDLEN,"%s IN_OPEN IN_ISDIR",name);
                else
                    snprintf(info.event,FIELDLEN,"%s IN_OPEN",name);
            else if ((event->mask & IN_ACCESS))
                if(event->mask & IN_ISDIR)
                    snprintf(info.event,FIELDLEN,"%s IN_ACCESS IN_ISDIR",name);
                else
                    snprintf(info.event,FIELDLEN,"%s IN_ACCESS",name);
            else if ((event->mask & IN_MODIFY))
                if(event->mask & IN_ISDIR)
                    snprintf(info.event,FIELDLEN,"%s IN_MODIFY IN_ISDIR",name);
                else
                    snprintf(info.event,FIELDLEN,"%s IN_MODIFY",name);
            else if ((event->mask & IN_CLOSE_WRITE))
                if(event->mask & IN_ISDIR)
                    snprintf(info.event,FIELDLEN,"%s IN_CLOSE_WRITE IN_ISDIR",name);
                else
                    snprintf(info.event,FIELDLEN,"%s IN_CLOSE_WRITE",name);
            else if ((event->mask & IN_CLOSE_NOWRITE))
                if(event->mask & IN_ISDIR)
                    snprintf(info.event,FIELDLEN,"%s IN_CLOSE_NOWRITE IN_ISDIR",name);
                else
                    snprintf(info.event,FIELDLEN,"%s IN_CLOSE_NOWRITE",name);
            else if ((event->mask & IN_MOVED_FROM))
                if(event->mask & IN_ISDIR)
                    snprintf(info.event,FIELDLEN,"%s IN_MOVED_FROM IN_ISDIR",name);
                else
                    snprintf(info.event,FIELDLEN,"%s IN_MOVED_FROM",name);
            else if ((event->mask & IN_MOVED_TO))
                if(event->mask & IN_ISDIR)
                    snprintf(info.event,FIELDLEN,"%s IN_MOVED_TO IN_ISDIR",name);
                else
                    snprintf(info.event,FIELDLEN,"%s IN_MOVED_TO",name);
            else if ((event->mask & IN_IGNORED)) {
                if(event->mask & IN_ISDIR)
                    snprintf(info.event,FIELDLEN,"%s IN_IGNORED IN_ISDIR",name);
                else
                    snprintf(info.event,FIELDLEN,"%s IN_IGNORED",name);
                info.terminate = 1; // File or directory was deleted!
            }
            else
                snprintf(info.event,FIELDLEN,"OTHER_EVENT");

            bytesProcessed += EVENT_STRUCT_SIZE + event->len;
        }
        gettimeofday(&current_time, NULL);
        snprintf(info.timestamp,FIELDLEN,"%ld.%ld", current_time.tv_sec, current_time.tv_usec);
        send(socket,info_ptr,sizeof(info),0);
    }
    return NULL;
}

void * obs_main(void * arg) {
	struct client_args cargs = *(struct client_args *)arg;
	void * cargs_ptr = &cargs;
	int sock = 0;
	struct sockaddr_in serv_addr;
	char *hello = "o";
	char buffer[BUFFER_SIZE] = {0};
    int error;
    pthread_t t;
    pthread_t u;
    struct eventinfo info;
    struct timeval current_time;
    void * info_ptr;
    
	if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		perror("socket failed");
		return NULL;
	}

	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(cargs.port);


	// Convert IPv4 and IPv6 addresses from text to binary form
	if(inet_pton(AF_INET, cargs.address, &serv_addr.sin_addr)<=0) {
		printf("\nInvalid address/ Address not supported \n");
		return NULL;
	}
	if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
		perror("connect");
		return NULL;
	}

    // Sends one last event to the server, and lets the server know that this client will be terminated
    (void) signal(SIGINT, catch);
    (void) signal(SIGTERM, catch);
	if(sigsetjmp(state,1)) {
        snprintf(info.monitored,FIELDLEN,"%s",cargs.filename);
        snprintf(info.host,FIELDLEN,"%s",cargs.address);
        snprintf(info.event,FIELDLEN+strlen(" IN_DELETE_SELF"),"%s IN_DELETE_SELF",cargs.filename);
        gettimeofday(&current_time, NULL);
        snprintf(info.timestamp,FIELDLEN,"%ld.%ld", current_time.tv_sec, current_time.tv_usec);
        info.terminate = 1;
        info_ptr = &info;
        send(sock,info_ptr,sizeof(info),0);
        return NULL;
    }

	cargs.port = sock;

    send(sock, hello, strlen(hello), 0 );
    error = pthread_create(&(t), NULL, &send_events, cargs_ptr); 
    if (error != 0) 
        printf("\nThread can't be created :[%s]", 
            strerror(error));

    // Runs in parallel to send events, will check to see if the server is still alive (server will send same code if just this process is terminated)
    error = pthread_create(&u,NULL,&check_is_alive,(void *)(intptr_t)sock);
    pthread_join(u, NULL);
	return NULL;
}

void * user_main(void *arg) {
	struct client_args cargs = *(struct client_args *)arg;
	int sock = 0;
	struct sockaddr_in serv_addr;
	char *hello = "u";
	char *goodbye = "c";
	char buffer[BUFFER_SIZE] = {0};
	void * num_events_ptr;
	int num_events;
	char * recent_events;
	char * header = "TIME\t\t\tHOST\t\tMONITORED\t\tEVENT";

	if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		perror("socket failed");
		return NULL;
	}

	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(cargs.port);

	// Convert IPv4 and IPv6 addresses from text to binary form
	if(inet_pton(AF_INET, cargs.address, &serv_addr.sin_addr)<=0) {
		printf("\nInvalid address/ Address not supported \n");
		return NULL;
	}
	if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
		perror("connect");
		return NULL;
	}

	(void) signal(SIGINT, catch);
	(void) signal(SIGTERM, catch);
	if(sigsetjmp(state,1)) {
		printf("Waiting for 1 last read, please wait...\n");
		read(sock, buffer, BUFFER_SIZE);
		send(sock, goodbye, strlen(goodbye), 0 );
		printf("Goodbye\n");
		return NULL;
	}

	num_events_ptr = (int *)malloc(sizeof(int));
	recent_events = (char *)malloc(FIELDLEN*4+4);
	system("clear");
	printf("%s\n",header);
	while(1) {
	
		send(sock, hello, strlen(hello), 0 );
		read(sock,num_events_ptr,sizeof(int));
		num_events = *(int *)num_events_ptr;
		if(num_events < 0) {
			return NULL; // This means the server is killing all threads
		}
		recent_events = (char *)realloc(recent_events,(FIELDLEN*4+4)*num_events);
		read(sock, recent_events, (FIELDLEN*4+4)*num_events);
		system("clear");
		printf("%s\n",header);
		printf("%s", recent_events);
	}

	return NULL;
}

void * server_main(void * arg) {
	struct server_args sargs = *(struct server_args *)arg;
	int server_fd, new_socket;
	int * sockets;
	struct sockaddr_in address;
	int opt = 1;
	int addrlen = sizeof(address);
	int error;
	int user_client_count = 1;
	int obs_client_count = 1;
	FILE * logfile;
	int writetologfile = 0;

	struct process_args pargs;
	void * pargs_ptr;

	pargs_ptr = & pargs;
	pargs.interval = sargs.interval;

	if(strlen(sargs.logfile)>0) {
		writetologfile = 1;
		logfile = fopen(sargs.logfile, "a");
		fputs("A new server is online\n",logfile);
	}

	if (pthread_mutex_init(&lock, NULL) != 0) { 
        printf("\n mutex init has failed\n"); 
        return NULL; 
    } 

	user_clients = (pthread_t * )malloc(sizeof(pthread_t));
	sockets = (int *)malloc(sizeof(int));

	if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
		perror("socket failed");
		exit(EXIT_FAILURE);
	}

	address.sin_family = AF_INET;
	address.sin_addr.s_addr = INADDR_ANY;
	address.sin_port = htons(sargs.port);

	if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
		perror("bind failed");
		exit(EXIT_FAILURE);
	}

    if (listen(server_fd, 3) < 0) {
		perror("listen");
		exit(EXIT_FAILURE);
	}


	// Send the shutdown signal (-1) to all observer and user clients
    (void) signal(SIGINT, catch);
    (void) signal(SIGTERM, catch);
	if(sigsetjmp(state,1)) {
		int killsig = -1;
		int * killsig_ptr = &killsig;
		for(int i=0;i<(user_client_count+obs_client_count-2);i++) {
			send(sockets[i],killsig_ptr,sizeof(int),0);
			shutdown(sockets[i],SHUT_WR);
		}
		free(user_clients);
		free(obs_clients);
		free(sockets);
		if(writetologfile == 1) {
			fputs("Server is shutting down\n",logfile);
			fclose(logfile);
			}
		return NULL;
	}

	while(1) {

		if ((new_socket = accept(server_fd, (struct sockaddr *)&address,
						(socklen_t*)&addrlen))<0) {
			perror("accept");
			exit(EXIT_FAILURE);
		}


		sockets = (int *)realloc(sockets,sizeof(int)*(user_client_count+obs_client_count-1));
		sockets[(user_client_count+obs_client_count)-2] = new_socket;
		pargs.socket = new_socket;

		read(new_socket, buffer, BUFFER_SIZE);
		//printf("%s\n",buffer);
		if(buffer[0]==117 && strlen(buffer)==1) {
			pargs.proc_id = user_client_count;
			user_clients = (pthread_t * ) realloc(user_clients, sizeof(pthread_t)*user_client_count);
			error = pthread_create(&(user_clients[user_client_count-1]), 
								NULL, 
								&serve_client, pargs_ptr); 
			if (error != 0) 
				printf("\nThread can't be created :[%s]", 
					strerror(error));

			if(writetologfile==1) {fputs("New user client connected\n",logfile);}
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

			//printf("%d observer client(s) connected!\n",obs_client_count);
			pthread_mutex_lock(&lock);
			critical_event_len = obs_client_count;
			pthread_mutex_unlock(&lock);
			if(writetologfile==1) {fputs("New observer client connected\n",logfile);}
			obs_client_count++;

		}

	}

    return NULL;
}

int main(int argc, char const *argv[]) {
	if(argc <4) {
		printf("Too few arguments supplied\n");
		return 1;
	}

	if(strcmp(argv[1],"-s") == 0) {
		if(strcmp(argv[2],"-t") != 0) {
			printf("Inverval flag (-t) not supplied\n");
			return 1;
		}
		struct server_args sargs;
		void * sargs_ptr;
		sargs_ptr = &sargs;
		sargs.interval = atof(argv[3]);
		if(argc >=6) {
			if(strcmp(argv[4],"-p") != 0) {
				printf("Port flag (-p) not supplied\n");
				return 1;
			}
			sargs.port = atoi(argv[5]);
			if(argc ==8) {
				if(strcmp(argv[6],"-l") != 0) {
					printf("Logfile flag (-l) not supplied\n");
					return 1;
				}
			snprintf(sargs.logfile,FIELDLEN,"%s",argv[7]);
			}
		}
		else {sargs.port = 8080;}
		server_main(sargs_ptr);
	}
	else if(strcmp(argv[1],"-u") == 0) {
		struct client_args cargs;
		void * cargs_ptr;
		cargs_ptr = &cargs;
		snprintf(cargs.address,FIELDLEN,"%s",argv[2]);
		cargs.port = atoi(argv[3]);
		user_main(cargs_ptr);
	}
	else if(strcmp(argv[1],"-o") == 0) {
		if(argc<5) {
			printf("Too few arguments supplied\n");
			return 1;
		}
		struct client_args cargs;
		void * cargs_ptr;
		cargs_ptr = &cargs;
		snprintf(cargs.address,FIELDLEN,"%s",argv[2]);
		cargs.port = atoi(argv[3]);
		snprintf(cargs.filename,FIELDLEN,"%s",argv[4]);
		obs_main(cargs_ptr);
	}
	else {
		printf("Unknown flag for argument 1 (expected -s, -u, or -o)\n");
	}
}