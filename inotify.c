#include <sys/inotify.h>
#include <stdio.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <limits.h>
#include <setjmp.h>
#include <signal.h>
#include <string.h>
#include <pthread.h>
#include <sys/time.h>
#include "notapp.h"

// create a buffer for at most 10 events
#define EVENT_STRUCT_SIZE sizeof(struct inotify_event) 
#define EVENT_BUFFER_SIZE (10 * (EVENT_STRUCT_SIZE + NAME_MAX + 1))
#define PORT 8080
#define BUFFER_SIZE 1024
#define EVENTS IN_CREATE|IN_DELETE|IN_ATTRIB|IN_OPEN|IN_ACCESS|IN_MODIFY|IN_CLOSE_WRITE|IN_CLOSE_NOWRITE|IN_MOVED_FROM|IN_MOVED_TO|IN_DELETE_SELF|IN_IGNORED
sigjmp_buf state;


// Call when CTRL-C is pressed
void catch() {
    siglongjmp(state,1);
}

int check_name(char * name) {
    for(int i=0;i<strlen(name);i++) {
        if(name[i] < 32 || name[i] > 127) {return 0;}
    }
    return 1;
}

void * send_events(void * arg) {
    int socket = (int)(intptr_t)arg;
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
    filename = "../E";
    // add a new watch to inotify_fd, monitor the current folder for file/directory creation and deletion
    // returns a watch descriptor. 
    int watch_des = inotify_add_watch(inotify_fd, filename,EVENTS);
    if(watch_des == -1) {
        perror ("inotify_add_watch");
        return NULL; // can't create the watch descriptor, return 1 to os and exit
    }

    snprintf(info.monitored,FIELDLEN,"%s",filename);
    snprintf(info.host,FIELDLEN,"%s","127.0.0.1");
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
                    snprintf(info.event,FIELDLEN,"%s IN_CREATE IN_ISDIR\n", name);
                else
                    snprintf(info.event,FIELDLEN,"%s IN_CREATE\n", name);
            else if ((event->mask & IN_DELETE))
                if (event->mask & IN_ISDIR)
                    snprintf(info.event,FIELDLEN,"%s IN_DELETE IN_ISDIR\n", name);
                else
                    snprintf(info.event,FIELDLEN,"%s IN_DELETE\n", name);
            else if ((event->mask & IN_ATTRIB))
                if(event->mask & IN_ISDIR)
                    snprintf(info.event,FIELDLEN,"%s IN_ATTRIB IN_ISDIR\n",name);
                else
                    snprintf(info.event,FIELDLEN,"%s IN_ATTRIB\n",name);
            else if ((event->mask & IN_OPEN))
                if(event->mask & IN_ISDIR)
                    snprintf(info.event,FIELDLEN,"%s IN_OPEN IN_ISDIR\n",name);
                else
                    snprintf(info.event,FIELDLEN,"%s IN_OPEN\n",name);
            else if ((event->mask & IN_ACCESS))
                if(event->mask & IN_ISDIR)
                    snprintf(info.event,FIELDLEN,"%s IN_ACCESS IN_ISDIR\n",name);
                else
                    snprintf(info.event,FIELDLEN,"%s IN_ACCESS\n",name);
            else if ((event->mask & IN_MODIFY))
                if(event->mask & IN_ISDIR)
                    snprintf(info.event,FIELDLEN,"%s IN_MODIFY IN_ISDIR\n",name);
                else
                    snprintf(info.event,FIELDLEN,"%s IN_MODIFY\n",name);
            else if ((event->mask & IN_CLOSE_WRITE))
                if(event->mask & IN_ISDIR)
                    snprintf(info.event,FIELDLEN,"%s IN_CLOSE_WRITE IN_ISDIR\n",name);
                else
                    snprintf(info.event,FIELDLEN,"%s IN_CLOSE_WRITE\n",name);
            else if ((event->mask & IN_CLOSE_NOWRITE))
                if(event->mask & IN_ISDIR)
                    snprintf(info.event,FIELDLEN,"%s IN_CLOSE_NOWRITE IN_ISDIR\n",name);
                else
                    snprintf(info.event,FIELDLEN,"%s IN_CLOSE_NOWRITE\n",name);
            else if ((event->mask & IN_MOVED_FROM))
                if(event->mask & IN_ISDIR)
                    snprintf(info.event,FIELDLEN,"%s IN_MOVED_FROM IN_ISDIR\n",name);
                else
                    snprintf(info.event,FIELDLEN,"%s IN_MOVED_FROM\n",name);
            else if ((event->mask & IN_MOVED_TO))
                if(event->mask & IN_ISDIR)
                    snprintf(info.event,FIELDLEN,"%s IN_MOVED_TO IN_ISDIR\n",name);
                else
                    snprintf(info.event,FIELDLEN,"%s IN_MOVED_TO\n",name);
            else if ((event->mask & IN_IGNORED)) {
                if(event->mask & IN_ISDIR)
                    snprintf(info.event,FIELDLEN,"%s IN_IGNORED IN_ISDIR\n",name);
                else
                    snprintf(info.event,FIELDLEN,"%s IN_IGNORED\n",name);
                info.terminate = 1; // File or directory was deleted!
            }
            else
                snprintf(info.event,FIELDLEN,"OTHER_EVENT\n");

            bytesProcessed += EVENT_STRUCT_SIZE + event->len;
        }
        gettimeofday(&current_time, NULL);
        snprintf(info.timestamp,FIELDLEN,"%ld.%ld", current_time.tv_sec, current_time.tv_usec);
        send(socket,info_ptr,sizeof(info),0);
    }
    return NULL;
}

int main(int argc, char const *argv[]) {
	int sock = 0;
	struct sockaddr_in serv_addr;
	char *hello = "o";
	char buffer[BUFFER_SIZE] = {0};
    int error;
    pthread_t t;
    struct eventinfo info;
    char * filename = "../E";
    struct timeval current_time;
    void * info_ptr;




	if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		perror("socket failed");
		return -1;
	}

	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(PORT);

	// Convert IPv4 and IPv6 addresses from text to binary form
	if(inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr)<=0) {
		printf("\nInvalid address/ Address not supported \n");
		return -1;
	}
	if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
		perror("connect");
		return -1;
	}

    // Sends one last event to the server, and lets the server know that this client will be terminated
    (void) signal(SIGINT, catch);
	if(sigsetjmp(state,1)) {
        snprintf(info.monitored,FIELDLEN,"%s",filename);
        snprintf(info.host,FIELDLEN,"%s","127.0.0.1");
        snprintf(info.event,FIELDLEN,"%s IN_DELETE_SELF",filename);
        gettimeofday(&current_time, NULL);
        snprintf(info.timestamp,FIELDLEN,"%ld.%ld", current_time.tv_sec, current_time.tv_usec);
        info.terminate = 1;
        info_ptr = &info;
        send(sock,info_ptr,sizeof(info),0);
        return 0;
    }


    send(sock, hello, strlen(hello), 0 );
    error = pthread_create(&(t), 
                    NULL, 
                    &send_events, (void *)(intptr_t)sock); 
    if (error != 0) 
        printf("\nThread can't be created :[%s]", 
            strerror(error));
    pthread_join(t, NULL);
	return 0;
}