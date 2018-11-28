#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>
#include <pthread.h>

#define BACKLOG 10
#define MAX_IP_SIZE 15
#define MAX_DATA_SIZE 100
#define MAX_MSG_SIZE 64
#define MAGIC_NUMBER 1248819489
#define MAX_BUF_LEN 75

struct Message {
	char groupID;
	int magicNum;
	char timeToLive;
	char ridDest;
	char ridSource;
	char message[64];
	char checksum;
};

struct JoinRequest {
	char groupID;
	int magicNum;
};

struct JoinResponse {
	char groupID;
	int magicNum;
	char yourRID;
	int nextSlaveIP;
};

// GLOBAL VARIABLES

int nextSlaveIP;
int nextSlaveID;
int portAsInt;
char port[5];
int nextSlavePort = 10045;

// MY FUNCTION DEFINITIONS

void stringToMessage(char *str, struct Message message, int numBytes);

void messageToString(struct Message message, char *str);

void stringToRequest(char str[], struct JoinRequest request);

void responseToString(char str[], struct JoinResponse response);

uint8_t computeChecksum(char *str, int length);

void *addNodes(void*);

void *handleMessages(void*);

void *forwardMessages(void*);

// BEEJ'S HELPER FUNCTIONS

void sigchld_handler(int s)
{
	// waitpid() might overwrite errno, so we save and restore it:
	int saved_errno = errno;

	while(waitpid(-1, NULL, WNOHANG) > 0);

	errno = saved_errno;
}

// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
	if (sa->sa_family == AF_INET) {
		return &(((struct sockaddr_in*)sa)->sin_addr);
	}

	return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

int main(int argc, char *argv[]) {
	pthread_t joinHandlerId;
	pthread_t messageHandlerId;
	pthread_t messageForwarderId;

	if (argc != 2) {
		// report missing port
		printf("missing port\n");
		exit(1);
	}

	strcpy(port, argv[1]);
	portAsInt = strtol(port, NULL, 0);
	if ((portAsInt < 1024) || (portAsInt > 65535)) {
		// throw port out of bounds error
		printf("port out of bounds\n");
		exit(1);
	}

	pthread_create(&joinHandlerId, NULL, addNodes, NULL);
	pthread_create(&messageHandlerId, NULL, handleMessages, NULL);
	pthread_create(&messageForwarderId, NULL, forwardMessages, NULL);
	pthread_join(joinHandlerId, NULL);
}

void *addNodes(void *vargp) {
	// SOCKET VARIABLES
	int sockfd_tcp, new_fd;  // listen on sock_fd, new connection on new_fd
	struct addrinfo hints, *addr_info, *servinfo, *p;
	struct sockaddr_storage their_addr; // connector's address information
	struct sockaddr my_addr;
	socklen_t sin_size;
	struct sigaction sa;
	int yes=1;
	char s[INET6_ADDRSTRLEN];
	int rv;
	char myName[1024];
	int myIPAsInt;
	char buf[MAX_BUF_LEN];
	int numBytes;

	// LAB VARIABLES
	char nextRID=1;
	struct JoinRequest request;
	struct JoinResponse response;

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;
	
	if ((rv = getaddrinfo(NULL, port, &hints, &servinfo)) != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
		return;
	}
	
	// loop through all the results and bind to the first we can
	for(p = servinfo; p != NULL; p = p->ai_next) {
		if ((sockfd_tcp = socket(p->ai_family, p->ai_socktype,
				p->ai_protocol)) == -1) {
			perror("master: socket");
			continue;
		}

		if (setsockopt(sockfd_tcp, SOL_SOCKET, SO_REUSEADDR, &yes,
				sizeof(int)) == -1) {
			perror("setsockopt");
			exit(1);
		}

		if (bind(sockfd_tcp, p->ai_addr, p->ai_addrlen) == -1) {
			close(sockfd_tcp);
			perror("master: bind");
			continue;
		}

		break;
	}

	freeaddrinfo(servinfo);

	if (p == NULL)  {
		fprintf(stderr, "master: failed to bind\n");
		exit(1);
	}

	if (listen(sockfd_tcp, BACKLOG) == -1) {
		perror("listen");
		exit(1);
	}

	sa.sa_handler = sigchld_handler; // reap all dead processes
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = SA_RESTART;
	if (sigaction(SIGCHLD, &sa, NULL) == -1) {
		perror("sigaction");
		exit(1);
	}

	gethostname(myName, 1024);
	getaddrinfo(myName, port, NULL, &addr_info);
	my_addr = *(addr_info->ai_addr);

	myIPAsInt = ((struct sockaddr_in*)&my_addr)->sin_addr.s_addr;
	nextSlaveIP = myIPAsInt;

	// printf("master: waiting for connections....\n");

	// receive request

	while(1) {  // main accept() loop
		sin_size = sizeof their_addr;
		new_fd = accept(sockfd_tcp, (struct sockaddr *)&their_addr, &sin_size);
		if (new_fd == -1) {
			perror("accept");
			continue;
		}

		inet_ntop(their_addr.ss_family,
			get_in_addr((struct sockaddr *)&their_addr),
			s, sizeof s);
		// printf("master: got connection from %s\n", s);

		// if problems, fork
		if ((numBytes = recv(new_fd, buf, MAX_DATA_SIZE-1, 0)) == -1) {
	    	perror("recv");
	   		exit(1);
		}

		buf[numBytes] = '\0';

		/*// printf("received: ");
		int i = 0;
		while(buf[i] != '\0') {
			// printf("%x ", buf[i++]);
		}
		// printf("\n");*/

		if (numBytes != 5) {
			// printf("master: received request of invalid size.\n");
			close(new_fd);
			continue;
		} else {
			//stringToRequest(buf, request);
			memcpy(&request.groupID, buf + 0, 1);
			memcpy(&request.magicNum, buf + 1, 4);
		}

		request.magicNum = ntohl(request.magicNum);

		if (request.magicNum != MAGIC_NUMBER) {
			// printf("master: received request with invalid magic number.\n");
			close(new_fd);
			continue;
		}

		//build response
		response.groupID = (char)7;
		response.magicNum = MAGIC_NUMBER;
		response.yourRID = nextRID++;
		response.nextSlaveIP = nextSlaveIP;

		//save this slave's IP as my master's next slave IP
		struct sockaddr_in *temp = (struct sockaddr_in *)&their_addr;
		//memcpy(nextSlaveIP, &temp->sin_addr.s_addr, 4);
		nextSlaveIP = temp->sin_addr.s_addr;
		memset(&temp, 0, sizeof temp);

		//send response back to requester
		memset(buf, 0, MAX_BUF_LEN);
		responseToString(&buf[0], response);
		buf[10] = '\0';

		/*// printf("sending: ");
		i = 0;
		while(buf[i] != '\0') {
			// printf("%x ", buf[i++]);
		}
		// printf("\n");*/

		nextSlavePort++;
		// printf("\nnextSlavePort == %d\n", nextSlavePort);

		if (send(new_fd, buf, 10, 0) == -1) {
			perror("send");
		}
		close(new_fd);
	}
}

void *handleMessages(void *vargp) {
	struct addrinfo hints, *servinfo, *p;
	int rv;
	int numbytes;
	int sockfd_udp_client;

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_DGRAM;

	char nextSlavePortAsString[5];
	sprintf(nextSlavePortAsString, "%d", nextSlavePort);

	struct in_addr nextSlaveIpStruct;
	memcpy(&nextSlaveIpStruct, &nextSlaveIP, 4);

	if ((rv = getaddrinfo(inet_ntoa(nextSlaveIpStruct),
		nextSlavePortAsString, &hints, &servinfo)) != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
		exit(1);
	}

	sockfd_udp_client = socket(AF_INET, SOCK_DGRAM, 0);

	/*
	// loop through all the results and make a socket
	for(p = servinfo; p != NULL; p = p->ai_next) {
		if ((sockfd_udp_client = socket(p->ai_family, p->ai_socktype,
				p->ai_protocol)) == -1) {
			perror("master: socket");
			continue;
		}

		break;
	} 
	*/

	int destination;
	char message[MAX_MSG_SIZE];
	char messageAsString[MAX_MSG_SIZE + 9];

	while(1) {
		memset(message, 0, sizeof message);
		memset(messageAsString, 0, MAX_MSG_SIZE + 9);
		printf("\nEnter message to send: ");
		scanf("%[^\n]", message);
		getchar();

		if (((strlen(message)) > 0) && (message[strlen(message) - 1] == '\n')) {
			message[strlen(message) - 1] = '\0';
		}

		printf("\nEnter ring ID of recipient: ");
		scanf("%d", &destination);
		getchar();

		int msgLength = strlen(message);

		// create message
		struct Message messageStruct;
		messageStruct.groupID = 7;
		messageStruct.magicNum = MAGIC_NUMBER;
		messageStruct.timeToLive = 255;
		messageStruct.ridDest = (char)destination;
		messageStruct.ridSource = (char)0;
		memcpy(&messageStruct.message, &message, msgLength);

		messageToString(messageStruct, &messageAsString[0]);
		// printf("\ncalculated 0x%x as checksum\n", computeChecksum(&messageAsString[0], msgLength + 8));
		messageAsString[msgLength + 8] = computeChecksum(&messageAsString[0], msgLength + 8);

		/*int i = 0;
		while (messageAsString[i] != '\0') {
			printf("%x ", (int)messageAsString[i]);
			i++;
		}
		printf("\n");*/

		// send to next slave
		struct sockaddr_in nextSlaveAddr;
		memset(&nextSlaveAddr, 0, sizeof nextSlaveAddr);
		nextSlaveAddr.sin_family = AF_INET;
		nextSlaveAddr.sin_port = htons(nextSlavePort);
		memcpy(&nextSlaveAddr.sin_addr.s_addr, &nextSlaveIP, 4);

		// printf("\nsending %d bytes to %s port %d\n",
		//	 strlen(messageAsString), inet_ntoa(nextSlaveAddr.sin_addr),
		// 	 htons(nextSlaveAddr.sin_port));

		numbytes = sendto(sockfd_udp_client, messageAsString, msgLength + 9, 0,
			(struct sockaddr *)&nextSlaveAddr, sizeof nextSlaveAddr);
		if (numbytes == -1) {
			perror("master: send");
			exit(1);
		}
	}
}

void *forwardMessages(void *vargp) {
	int sockfd_udp_server;

	struct addrinfo hints, *servinfo, *p;
	int rv;
	struct sockaddr_storage their_addr;
	socklen_t addr_len;
	char s[INET6_ADDRSTRLEN];

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_DGRAM;
	hints.ai_flags = AI_PASSIVE; // use my IP

	if ((rv = getaddrinfo(NULL, "10045", &hints, &servinfo)) != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
		return;
	}

	// loop through all the results and bind to the first we can
	for(p = servinfo; p != NULL; p = p->ai_next) {
		if ((sockfd_udp_server = socket(p->ai_family, p->ai_socktype,
				p->ai_protocol)) == -1) {
			perror("listener: socket");
			continue;
		}

		if (bind(sockfd_udp_server, p->ai_addr, p->ai_addrlen) == -1) {
			close(sockfd_udp_server);
			perror("listener: bind");
			continue;
		}

		break;
	}

	if (p == NULL) {
		fprintf(stderr, "listener: failed to bind socket\n");
		return;
	}

	freeaddrinfo(servinfo);

	while(1) {
		char buf[MAX_MSG_SIZE + 9];
		struct sockaddr_storage prevSlaveInfo;
		int addressLength = sizeof prevSlaveInfo;
		struct Message messageStruct;

		int size = recvfrom(sockfd_udp_server, buf, MAX_MSG_SIZE + 8, 0,
			(struct sockaddr *)&prevSlaveInfo, &addressLength);
		if (size == -1) {
			perror("error receiving");
			exit(1);
		}

		/* printf("\nreceived: ");
		int i = 0;
		for (i; i < size; i++) {
			printf("%x ", buf[i]);
		}
		printf("\n");*/

		if (buf[5] == 1) {
			printf("\nMessage expired.\n");
			continue;
		}

		if (buf[6] != '\0') {
			printf("\nmessage not for me\n");
			// send to next slave

			buf[5] = buf[5] - 1;
			buf[size - 1]++;

			struct sockaddr_in nextSlaveAddr;
			memset(&nextSlaveAddr, 0, sizeof nextSlaveAddr);
			nextSlaveAddr.sin_family = AF_INET;
			nextSlaveAddr.sin_port = htons(nextSlavePort);
			memcpy(&nextSlaveAddr.sin_addr.s_addr, &nextSlaveIP, 4);

			int numbytes = sendto(sockfd_udp_server, buf, size, 0,
				(struct sockaddr *)&nextSlaveAddr, sizeof nextSlaveAddr);
			if (numbytes == -1) {
				perror("master: send");
				exit(1);
			}
			continue;
		}

		if ((0xFF&buf[size - 1]) != computeChecksum(&buf[0], size - 1)) {
			printf("\nchecksums do not match\n");
			// printf("\nreceived 0x%x\n", 0xFF&buf[size - 1]);
			// printf("\ncalculated 0x%x\n", computeChecksum(&buf[0], size - 1));
			continue;
		}

		int i;
		printf("\nYou've got mail!\n");
		printf("%c", 0x07);
		for (i = 8; i < size - 1; i++) {
			printf("%c", buf[i]);
		}
		printf("\n");
	}
}

void messageToString(struct Message messageStruct, char *str) {
	messageStruct.magicNum = htonl(messageStruct.magicNum);
	
	memcpy(str + 0, &messageStruct.groupID, 1);
	memcpy(str + 1, &messageStruct.magicNum, 4);
	memcpy(str + 5, &messageStruct.timeToLive, 1);
	memcpy(str + 6, &messageStruct.ridDest, 1);
	memcpy(str + 7, &messageStruct.ridSource, 1);
	int i = 0;
	while (messageStruct.message[i] != '\0') {
		memcpy(str + 8 + i, &messageStruct.message[i], 1);
		i++;
	}
}

void stringToMessage(char *str, struct Message message, int numBytes) {
	int msgLength = numBytes - 9;

	memcpy(&message.groupID, str + 0, 1);
	memcpy(&message.magicNum, str + 1, 4);
	memcpy(&message.timeToLive, str + 5, 1);
	memcpy(&message.ridDest, str + 6, 1);
	memcpy(&message.ridSource, str + 7, 1);
	memcpy(&message.message, str + 8, msgLength);
	memcpy(&message.checksum, str + numBytes - 1, 1);

	message.magicNum = ntohl(message.magicNum);
}

void stringToRequest(char str[], struct JoinRequest request) {
	memcpy(&request.groupID, str + 0, 1);
	memcpy(&request.magicNum, str + 1, 4);
}

void responseToString(char *str, struct JoinResponse response) {
	response.magicNum = htonl(response.magicNum);
	response.groupID = 7;
	memcpy(str + 0, &response.groupID, 1);
	memcpy(str + 1, &response.magicNum, 4);
	memcpy(str + 5, &response.yourRID, 1);
	memcpy(str + 6, &response.nextSlaveIP, 4);
}

uint8_t computeChecksum(char *str, int length) {
	uint16_t acc = 0xFF;

	int i = 0;
	for (i = 0; i < length; i++) {
		uint8_t byte;
		memcpy(&byte, str + i, 1);
		acc += byte;
		acc = (acc & 0xFF) + (acc >> 8);
	}

	return ~acc;
}