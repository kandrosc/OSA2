#include <unistd.h>
#include <stdio.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <string.h>

#define PORT        8080
#define BUFFER_SIZE 1024

int main(int argc, char const *argv[]) {
	int server_fd, new_socket;
	struct sockaddr_in address;
	int opt = 1;
	int addrlen = sizeof(address);
	char buffer[BUFFER_SIZE] = {0};
	char *hello = "Hello from server, this is greeting number %d!";

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
	int counter = 1;
	char resp[strlen(hello) + 16];
	while(1) {
	if ((new_socket = accept(server_fd, (struct sockaddr *)&address,
					(socklen_t*)&addrlen))<0) {
		perror("accept");
		exit(EXIT_FAILURE);
	}

    read(new_socket, buffer, BUFFER_SIZE);
	printf("%s\n", buffer);

	snprintf(resp,sizeof(resp),hello,counter);
    send(new_socket ,resp ,strlen(resp), 0);
	printf("Hello message sent\n");
	counter++;
	}
    return 0;
}
