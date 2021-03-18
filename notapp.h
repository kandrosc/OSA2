#define FIELDLEN 128
struct eventinfo {
    char timestamp[FIELDLEN];
    char host[FIELDLEN];
    char monitored[FIELDLEN];
    char event[FIELDLEN];
    int terminate;
};

struct process_args {
    int socket;
    int proc_id;
    double interval;
};

struct server_args {
    double interval;
    int port;
    char logfile[FIELDLEN];
};

struct client_args {
    char address[FIELDLEN];
    int port;
    char filename[FIELDLEN];
};