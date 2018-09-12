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

struct client_request {
	char msgLength;
	char requestID;
	char opCode;
	char numOperands;
	short op1;
	short op2;
};

struct server_response {
	char msgLength;
	char requestID;
	char errorCode;
	int result;
};

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
	struct client_request request;
	struct server_response response;
	char responseAsArray[7];
	
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
	
	printf("packet is %d bytes long\n", sizeInBytes);
	buf[sizeInBytes] = '\0';
	printf("listener: packet contains ");
	int j;
	for(j = 0; j < strlen(buf); j++) {
		printf("%d, ", (int)buf[j]);
	}
	printf("\n");
	
	memcpy(&request.msgLength, buf + 0, 1);
	memcpy(&request.requestID, buf + 1, 1);
	memcpy(&request.opCode, buf + 2, 1);
	memcpy(&request.numOperands, buf + 3, 1);
	memcpy(&request.op1, buf + 4, 2);
	
	if ((int)request.msgLength > 6) {
		memcpy(&request.op2, buf + 6, 2);
	}
	
	request.op1 = ntohs(request.op1);
	request.op2 = ntohs(request.op2);
	
	memset(&response, 0, sizeof response);
	response.msgLength = 0x07;
	response.requestID = request.requestID;
	response.errorCode = 0x00;
	switch(request.opCode) {
		case 0x00:
			response.result = request.op1 + request.op2;
			break;
		case 0x01:
			response.result = request.op1 - request.op2;
			break;
		case 0x02:
			response.result = request.op1 | request.op2;
			break;
		case 0x03:
			response.result = request.op1 & request.op2;
			break;
		case 0x04:
			response.result = request.op1 >> request.op2;
			break;
		case 0x05:
			response.result = request.op1 << request.op2;
			break;
		case 0x06:
			response.result = ~request.op1;
			break;
		default:
			response.errorCode = (char)127;
	}
	
	response.result = htonl(response.result);
	
	memcpy(responseAsArray + 0, &response.msgLength, 1);
	memcpy(responseAsArray + 1, &response.requestID, 1);
	memcpy(responseAsArray + 2, &response.errorCode, 1);
	memcpy(responseAsArray + 3, &response.result, 4);
	
	/* printf("response message length: %d\n", response.msgLength);
	printf("response requestID: %d\n", response.requestID);
	printf("response error code: %d\n", response.errorCode);
	printf("request operand 1: %d\n", request.op1);
	printf("request operand 2: %d\n", request.op2);
	printf("response response: %d\n", response.result); */
	
	/* int i;
	for (i = 0; i < 7; i++) {
		printf("%d\n", (int)responseAsArray[i]);
	} */
	
	sizeInBytes = sendto(sockFileDesc, responseAsArray, 
		response.msgLength, 0, (struct sockaddr *)&clientInfo,
		addressLength);
	if (sizeInBytes == -1) {
		perror("error sending\n");
		exit(1);
	}
	
	close(sockFileDesc);
}