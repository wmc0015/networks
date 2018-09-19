/*
** client.c -- a stream socket client demo
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

#define PORT "10010" // the port client will be connecting to

#define MAXDATASIZE 100 // max number of bytes we can get at once

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

 	struct PacketOut
	{
		unsigned int tml: 8; // total message length
		unsigned int rid: 8; // request id
		unsigned int opcode: 8; // operation code
		unsigned int num_operands: 8; // number of operands
		signed int op1: 16; // operand 1
		signed int op2: 16; // operand 2
	} __attribute__((__packed__));

	struct PacketIn
	{
		unsigned int tml: 8; // total message length
		unsigned int rid: 8; // request id
		unsigned int error_code: 8; // error code
		unsigned int response: 32; // response
	} __attribute__((__packed__));

	typedef struct PacketOut PacketOut;
	typedef struct PacketIn  PacketIn;

	if (argc != 3) {
	    fprintf(stderr,"usage: client hostname PortNumber \n");
	    exit(1);
	}

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

	if ((rv = getaddrinfo(argv[1], PORT, &hints, &servinfo)) != 0) {
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

	printf("Enter an operation code: ");
	scanf("%d", &opcode);
	printf("Enter operand 1: ");
	scanf("%d", &op1);

	PacketOut *packet = malloc(sizeof(PacketOut));

	if (opcode != 6)	{ // only unary operation
		printf("Enter operand 2: ");
		scanf("%d", &op2);
		packet->tml = 8;
		packet->num_operands = 2;
		packet->op2 = htons(op2);
	}
	else {
		packet->tml = 6;
		packet->num_operands = 1;
	}

	packet->op1 = htons(op1);
	packet->rid = rand() % 257;
	packet->opcode = opcode;

	if ((send(sockfd, packet, packet->tml, 0)) == -1)	{
		perror("send");
	}
	else {
		printf("Packet sent: %X %X %X %X %X %X\n", packet->tml, packet->rid, packet->opcode, packet->num_operands, packet->op1, packet->op2);
	}

	PacketIn *packet_in = malloc(sizeof(PacketIn));



	if ((numbytes = recv(sockfd, packet_in, MAXDATASIZE-1, 0)) == -1) {
	    perror("recv");
	    exit(1);
	}
	else {
		printf("Packet received!\n");
	}

	buf[numbytes] = '\0';

	printf("client: received '%d %d %d %d'\n", packet_in->tml, packet_in->rid, packet_in->error_code, packet_in->response);

	close(sockfd);

	return 0;
}
