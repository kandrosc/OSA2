#include <stdio.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <setjmp.h>
#include <signal.h>

#define PORT 8080
#define BUFFER_SIZE 1024

sigjmp_buf state;

// Call when CTRL-C is pressed
void catch() {
    siglongjmp(state,1);
}

int main(int argc, char const *argv[]) {
	int sock = 0;
	struct sockaddr_in serv_addr;
	char *hello = "u";
	char *goodbye = "c";
	char buffer[BUFFER_SIZE] = {0};

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

	(void) signal(SIGINT, catch);
	if(sigsetjmp(state,1)) {
		printf("Waiting for 1 last read, please wait...\n");
		read(sock, buffer, BUFFER_SIZE);
		send(sock, goodbye, strlen(goodbye), 0 );
		printf("Goodbye\n");
		return 0;
	}

	while(1) {

    send(sock, hello, strlen(hello), 0 );

    read(sock, buffer, BUFFER_SIZE);
	printf("%s\n", buffer);
	}

	return 0;
}
