#include <stdio.h>
#include <unistd.h>
#include <sys/inotify.h>

#define EVENTS IN_OPEN | IN_ACCESS | IN_MODIFY | IN_ATTRIB | IN_CREATE | IN_DELETE | IN_DELETE_SELF | IN_MOVED_TO | IN_MOVED_FROM | IN_CLOSE_WRITE | IN_CLOSE_NOWRITE
#define EVENT_SIZE  (sizeof (struct inotify_event))
#define BUF_LEN (1024 * (EVENT_SIZE + 16))

int main() {
    int fd;
    int wd;
    char buf[BUF_LEN];
    int len;
    int i;


    fd = inotify_init ();
    if (fd < 0) {
        perror ("inotify_init");
    }


    wd = inotify_add_watch (fd, "../example.txt",EVENTS);

    if (wd < 0) {
        perror ("inotify_add_watch");
    }

    len = read (fd, buf, BUF_LEN);

    while (i < len) {
            struct inotify_event *event;

            event = (struct inotify_event *) &buf[i];

            printf ("wd=%d mask=%u cookie=%u len=%u\n",
                    event->wd, event->mask,
                    event->cookie, event->len);

            if (event->len)
                    printf ("name=%s\n", event->name);

            i += EVENT_SIZE + event->len;
    }

}