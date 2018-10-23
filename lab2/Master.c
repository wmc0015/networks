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

#define BACKLOG 10
#define MAX_IP_SIZE 15
#define MAX_DATA_SIZE 100
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

// MY FUNCTION DEFINITIONS

void stringToMessage(char str[], struct Message message, int numBytes);

void stringToRequest(char str[], struct JoinRequest request);

void responseToString(char str[], struct JoinResponse response);

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
	// SOCKET VARIABLES
	int sockfd, new_fd;  // listen on sock_fd, new connection on new_fd
	struct addrinfo hints, *addr_info, *servinfo, *p;
	struct sockaddr_storage their_addr; // connector's address information
	struct sockaddr my_addr;
	socklen_t sin_size;
	struct sigaction sa;
	int yes=1;
	char s[INET6_ADDRSTRLEN];
	int rv;
	int portAsInt;
	char port[5];
	char myName[1024];
	int myIPAsInt;
	char buf[MAX_BUF_LEN];
	int numBytes;

	// LAB VARIABLES
	char nextRID=1;
	int nextSlaveIP;
	int nextSlaveID;
	struct JoinRequest request;
	struct JoinResponse response;
	
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

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;
	
	if ((rv = getaddrinfo(NULL, port, &hints, &servinfo)) != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
		return 1;
	}
	
	// loop through all the results and bind to the first we can
	for(p = servinfo; p != NULL; p = p->ai_next) {
		if ((sockfd = socket(p->ai_family, p->ai_socktype,
				p->ai_protocol)) == -1) {
			perror("master: socket");
			continue;
		}

		if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes,
				sizeof(int)) == -1) {
			perror("setsockopt");
			exit(1);
		}

		if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
			close(sockfd);
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

	if (listen(sockfd, BACKLOG) == -1) {
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
	printf("master IP: %X\n", htonl(myIPAsInt));	

	printf("master: waiting for connections...\n");

	// receive request

	while(1) {  // main accept() loop
		sin_size = sizeof their_addr;
		new_fd = accept(sockfd, (struct sockaddr *)&their_addr, &sin_size);
		if (new_fd == -1) {
			perror("accept");
			continue;
		}

		inet_ntop(their_addr.ss_family,
			get_in_addr((struct sockaddr *)&their_addr),
			s, sizeof s);
		printf("master: got connection from %s\n", s);

		// if problems, fork
		if ((numBytes = recv(new_fd, buf, MAX_DATA_SIZE-1, 0)) == -1) {
	    	perror("recv");
	   		exit(1);
		}

		buf[numBytes] = '\0';

		printf("received: ");
		int i = 0;
		while(buf[i] != '\0') {
			printf("%x ", buf[i++]);
		}
		printf("\n");

		if (numBytes != 5) {
			printf("master: received request of invalid size.\n");
			close(new_fd);
			continue;
		} else {
			//stringToRequest(buf, request);
			printf("numBytes == 5\n");
			memcpy(&request.groupID, buf + 0, 1);
			printf("copied GID\n");
			memcpy(&request.magicNum, buf + 1, 4);
			printf("copied magic num\n");			
			printf("gid: %c\n", request.groupID);
			printf("magic num: %d\n", request.magicNum);
		}

		request.magicNum = ntohl(request.magicNum);

		if (request.magicNum != MAGIC_NUMBER) {
			printf("master: received request with invalid magic number.\n");
			close(new_fd);
			continue;
		}

		//build response
		response.groupID = (char)7;
		response.magicNum = MAGIC_NUMBER;
		response.yourRID = nextRID++;
		response.nextSlaveIP = nextSlaveIP;

		printf("response IP address: %x\n", response.nextSlaveIP);

		//save this slave's IP as my master's next slave IP
		struct sockaddr_in *temp = (struct sockaddr_in *)&their_addr;
		//memcpy(nextSlaveIP, &temp->sin_addr.s_addr, 4);
		nextSlaveIP = temp->sin_addr.s_addr;
		memset(&temp, 0, sizeof temp);

		//send response back to requester
		memset(buf, 0, MAX_BUF_LEN);
		responseToString(&buf[0], response);
		buf[10] = '\0';

		printf("sending: ");
		i = 0;
		while(buf[i] != '\0') {
			printf("%x ", buf[i++]);
		}
		printf("\n");

		if (send(new_fd, buf, 10, 0) == -1) {
			perror("send");
		}
		close(new_fd);
	}
}

void stringToMessage(char str[], struct Message message, int numBytes) {
	int msgLength = numBytes - 9;

	memcpy(&message.groupID, str + 0, 1);
	memcpy(&message.magicNum, str + 1, 4);
	memcpy(&message.timeToLive, str + 5, 1);
	memcpy(&message.ridDest, str + 6, 1);
	memcpy(&message.ridSource, str + 7, 1);
	memcpy(&message.message, str + 7, msgLength);
	memcpy(&message.checksum, str + numBytes - 1, 1);
}

void stringToRequest(char str[], struct JoinRequest request) {
	memcpy(&request.groupID, str + 0, 1);
	memcpy(&request.magicNum, str + 1, 4);
}

void responseToString(char *str, struct JoinResponse response) {
	response.magicNum = htonl(response.magicNum);
	printf("response.nextSlaveIP: %x\n", htonl(response.nextSlaveIP));
	response.groupID = 7;
	memcpy(str + 0, &response.groupID, 1);
	memcpy(str + 1, &response.magicNum, 4);
	memcpy(str + 5, &response.yourRID, 1);
	memcpy(str + 6, &response.nextSlaveIP, 4);
}