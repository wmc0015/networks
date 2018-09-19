/*
** TCP client
** Author: Alexander King
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#define MAXDATASIZE 100 // max number of bytes we can get at once

// packet sent to server
struct Request {
	unsigned char tml; // total message length
	unsigned char rid; // request id
	unsigned char opcode; // operation code
	unsigned char num_operands; // number of operands
	short op1; // operand 1
	short op2; // operand 2
} __attribute__((__packed__));

// packet received from server
struct Response {
	unsigned char tml;
	unsigned char rid;
	unsigned char error_code;
	int response;
} __attribute__((__packed__));

// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
	if (sa->sa_family == AF_INET) {
		return &(((struct sockaddr_in*)sa)->sin_addr);
	}

	return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

int main(int argc, char *argv[])
{
	int sockfd, numbytes;
	char buf[MAXDATASIZE];
	struct addrinfo hints, *servinfo, *p;
	int rv;
	char s[INET6_ADDRSTRLEN];
	int opcode;
	int op1;
	int op2;
	struct timeval start, end;

	if (argc != 3) {
	    fprintf(stderr,"usage: client <hostname> <PortNumber> \n");
	    exit(1);
	}

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

	if ((rv = getaddrinfo(argv[1], argv[2], &hints, &servinfo)) != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
		return 1;
	}

	// loop through all the results and connect to the first we can
	for(p = servinfo; p != NULL; p = p->ai_next) {
		if ((sockfd = socket(p->ai_family, p->ai_socktype,
				p->ai_protocol)) == -1) {
			perror("client: socket");
			continue;
		}

		if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
			close(sockfd);
			perror("client: connect");
			continue;
		}

		break;
	}

	if (p == NULL) {
		fprintf(stderr, "client: failed to connect\n");
		return 2;
	}

	inet_ntop(p->ai_family, get_in_addr((struct sockaddr *)p->ai_addr),
			s, sizeof s);
	printf("client: connecting to %s\n", s);

	freeaddrinfo(servinfo); // all done with this structure

	// send the message
	while(1) {
		struct Request request;
		int opcode, op1, op2;
		printf("Enter an operation code (0-6 or -1 to quit): ");
		scanf("%d", &opcode);

		if (opcode == -1)	{
			printf("Shutting down...\n");
			exit(0);
		}

		printf("Enter operand 1: ");
		scanf("%d", &op1);

		// binary operation
		if (opcode != 6) {
			printf("Enter operand 2: ");
			scanf("%d", &op2);

			request.op2 = htons(op2);
			request.num_operands = 2;
		}
		else {
			request.num_operands = 1;
		}

		request.op1 = htons(op1);
		request.opcode = opcode;
		request.rid = rand();
		request.tml = sizeof(request);

		gettimeofday(&start, NULL);
		send(sockfd, &request, request.tml, 0);

		// receive the response
		struct Response response;
		recv(sockfd, &response, MAXDATASIZE-1, 0);
		gettimeofday(&end, NULL);

		int result = ntohl(response.response);

		suseconds_t elapsed_time = (end.tv_usec - start.tv_usec);


		printf("Request ID: %d\n", response.rid);
		printf("Response: %d\n", result);
		printf("Elapsed Time: %d microseconds\n", elapsed_time); // convert to miliseconds from microseconds

	}

	close(sockfd);

	return 0;
}
