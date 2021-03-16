#include <sys/inotify.h>
#include <stdio.h>
#include <unistd.h>
#include <limits.h>
#include <setjmp.h>
#include <signal.h>
#include <string.h>

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

int main() {
    char * filename;
    char * name;
    // create a new inotify file descriptor
    // it has an associated watch list
    // read from it to get events
    int inotify_fd = inotify_init();
    if(inotify_fd < 0) {
        perror ("inotify_init");
        return 1; // can't create the inotify fd, return 1 to os and exit
    }
    filename = "../E";
    // add a new watch to inotify_fd, monitor the current folder for file/directory creation and deletion
    // returns a watch descriptor. 
    int watch_des = inotify_add_watch(inotify_fd, filename,EVENTS);
    if(watch_des == -1) {
        perror ("inotify_add_watch");
        return 1; // can't create the watch descriptor, return 1 to os and exit
    }

    // create a buffer for at most 10 events
    #define EVENT_STRUCT_SIZE sizeof(struct inotify_event) 
    #define BUFFER_SIZE (10 * (EVENT_STRUCT_SIZE + NAME_MAX + 1))
    char buffer[BUFFER_SIZE];

	(void) signal(SIGINT, catch);
	if(sigsetjmp(state,1)) {
        inotify_rm_watch(inotify_fd, watch_des);
        close(inotify_fd);
        printf("%s IN_DELETE_SELF\n",filename);
        return 0;
    }
    // start to monitor
    while(1) {
        // read 
        int bytesRead = read(inotify_fd, buffer, BUFFER_SIZE), bytesProcessed = 0;
        if(bytesRead < 0) { // read error
            perror("read");
            return 1;
        }

        while(bytesProcessed < bytesRead) {
            struct inotify_event* event = (struct inotify_event*)(buffer + bytesProcessed);
            if(check_name(event->name) == 1) {name = event->name;}
            else {name = filename;}
            if (event->mask & IN_CREATE)
                if (event->mask & IN_ISDIR)
                    printf("%s IN_CREATE IN_ISDIR\n", name);
                else
                    printf("%s IN_CREATE\n", name);
            else if ((event->mask & IN_DELETE))
                if (event->mask & IN_ISDIR)
                    printf("%s IN_DELETE IN_ISDIR\n", name);
                else
                    printf("%s IN_DELETE\n", name);
            else if ((event->mask & IN_ATTRIB))
                if(event->mask & IN_ISDIR)
                    printf("%s IN_ATTRIB IN_ISDIR\n",name);
                else
                    printf("%s IN_ATTRIB\n",name);
            else if ((event->mask & IN_OPEN))
                if(event->mask & IN_ISDIR)
                    printf("%s IN_OPEN IN_ISDIR\n",name);
                else
                    printf("%s IN_OPEN\n",name);
            else if ((event->mask & IN_ACCESS))
                if(event->mask & IN_ISDIR)
                    printf("%s IN_ACCESS IN_ISDIR\n",name);
                else
                    printf("%s IN_ACCESS\n",name);
            else if ((event->mask & IN_MODIFY))
                if(event->mask & IN_ISDIR)
                    printf("%s IN_MODIFY IN_ISDIR\n",name);
                else
                    printf("%s IN_MODIFY\n",name);
            else if ((event->mask & IN_CLOSE_WRITE))
                if(event->mask & IN_ISDIR)
                    printf("%s IN_CLOSE_WRITE IN_ISDIR\n",name);
                else
                    printf("%s IN_CLOSE_WRITE\n",name);
            else if ((event->mask & IN_CLOSE_NOWRITE))
                if(event->mask & IN_ISDIR)
                    printf("%s IN_CLOSE_NOWRITE IN_ISDIR\n",name);
                else
                    printf("%s IN_CLOSE_NOWRITE\n",name);
            else if ((event->mask & IN_MOVED_FROM))
                if(event->mask & IN_ISDIR)
                    printf("%s IN_MOVED_FROM IN_ISDIR\n",name);
                else
                    printf("%s IN_MOVED_FROM\n",name);
            else if ((event->mask & IN_MOVED_TO))
                if(event->mask & IN_ISDIR)
                    printf("%s IN_MOVED_TO IN_ISDIR\n",name);
                else
                    printf("%s IN_MOVED_TO\n",name);
            else if ((event->mask & IN_IGNORED)) {
                if(event->mask & IN_ISDIR)
                    printf("%s IN_IGNORED IN_ISDIR\n",name);
                else
                    printf("%s IN_IGNORED\n",name);
                return 0; // File or directory was deleted!
            }
            else
                printf("OTHER_EVENT\n");

            bytesProcessed += EVENT_STRUCT_SIZE + event->len;
        }
    }

}