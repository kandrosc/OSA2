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