#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#define MAXBUFLEN 100

int main(int argc, char *argv[]) {
	char port[5];
	int portAsInt;
	struct addrinfo hints, *serverInfo, *addrInfo;
	struct sockaddr_storage clientInfo;
	socklen_t addressLength;
	int sockFileDesc;
	int error;
	int sizeInBytes;
	char buf[MAXBUFLEN];
	
	if (argc != 2) {
		// report missing port
		printf("missing port\n");
		exit(1);
	}
	
	strcpy(port, argv[1]);
	portAsInt = strtol(port, NULL, 0);
	if ((portAsInt < 1024) || (portAsInt > 65535)) {
		// throw port out of bounds error
		printf("out of bounds\n");
		exit(1);
	}
	
	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_DGRAM;
	hints.ai_flags = AI_PASSIVE;
	
	error = getaddrinfo(NULL, port, &hints, &serverInfo);
	if (error != 0) {
		printf("some error %d", error);
		exit(1);
	}
	
	addrInfo = serverInfo;
	while(addrInfo != NULL) {
		sockFileDesc = socket(addrInfo->ai_family,
			addrInfo->ai_socktype, addrInfo->ai_protocol);
		if (sockFileDesc == -1) {
			addrInfo = addrInfo->ai_next;
			continue;
		}
		
		error = bind(sockFileDesc, addrInfo->ai_addr,
			addrInfo->ai_addrlen);
		if (error == -1) {
			close(sockFileDesc);
			addrInfo = addrInfo->ai_next;
			continue;
		}
		
		break;
	}
	
	if (addrInfo == NULL) {
		fprintf(stderr, "failed to bind to socket\n");
		return 1;
	}
	
	freeaddrinfo(serverInfo);
	
	printf("waiting for request from client...\n");
	
	addressLength = sizeof clientInfo;
	sizeInBytes = recvfrom(sockFileDesc, buf, MAXBUFLEN-1, 0,
		(struct sockaddr *)&clientInfo, &addressLength);
	if (sizeInBytes == -1) {
		perror("error receiving");
		exit(1);
	}
}