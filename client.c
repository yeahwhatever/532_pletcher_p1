/**
 * Joe Pletcher
 * CIS 532 Programming Project 1
 *
 * client.c
 *
 * Sources:
 * Beej's network programming guide
 * 	-http://beej.us/guide/bgnet/output/html/singlepage/bgnet.html
 *
 * Nonblocking User Input
 * 	-http://cc.byexamples.com/2007/04/08/non-blocking-user-input-in-loop-without-ncurses/
 *
 */

#define DEBUG 2

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#include "client.h"

int main(int argc, char *argv[]) {
	char *host, *port, *nick;
	int socketfd, status;
	struct addrinfo hints, *servinfo, *p;

	// Get/store command line args
	if (argc != 4) {
		printf("Usage: %s <server> <port> <nick>\n", argv[0]);
		return 1;
	} else {
		host = argv[1];
		port = argv[2];
		nick = argv[3];
	}


	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_DGRAM;

	if ((status = getaddrinfo(host, port, &hints, &servinfo)) != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(status));
		return 2;
	}

#if DEBUG > 1
	printf("DEBUG: getaddrinfo worked\n");
#endif

	for (p = servinfo; p != NULL; p = p->ai_next) {
		// If we cant create the socket, error and continue
		// otherwise break
		if ((socketfd = socket(p->ai_family, p->ai_socktype,
			p->ai_protocol)) == -1) {
			perror("client: call to socket failed");
			continue;
		} else {
			break;
		}
	}

	// Set non-blocking
	//fcntl(socketfd, F_SETFL, O_NONBLOCK);

	if (p == NULL) {
		fprintf(stderr, "client: failed to make socket\n");
		return 3;
	}

#if DEBUG > 1
	printf("DEBUG: socket worked\n");
#endif


	client_login(socketfd, p, nick);

	status = ui_loop(socketfd, p);

	// Clean Up
	freeaddrinfo(servinfo);
	close(socketfd);

	return status; 
}

int ui_loop(int socketfd, struct addrinfo *p) {
	char input[65]; //64 bytes we can send, plus newline
	char payload[4+32+64]; // 32 bit header, 32 byte first field, payload
	char channel[32] = "Common";
	int num;
	unsigned int i;

	do {
		printf("> ");
		fflush(stdin);
		while (!kbhit()) {
			
			usleep(1);
		}
		fgets(input, sizeof(input), stdin);

		//for (i = 0; i < strlen(input); i++) {
		//	// Backspace away the input on enter
		//	printf("\b");
		//}

#if DEBUG > 1
		//printf("DEBUG: input |%s|\n", input);
#endif

		client_prepare(input, payload, channel);

		i = payload_size(payload);

		if (!i) {
			// If payload size is 0, something went wrong.
#if DEBUG > 0
			printf("DEBUG: error in payload, size 0");
#endif
			continue;
		}

		if ((num = sendto(socketfd, payload, i, 0,
			p->ai_addr, p->ai_addrlen)) == -1) {
			perror("client: sendto failed\n");
			return 4;
		}

#if DEBUG > 0
		printf("DEBUG: sent %d bytes\n", num);
#endif


	} while (strcmp(input, "/exit"));

	return 0;
}

void client_prepare(char *input, char *payload, char *channel) {
	char command[7];
	char field2[32], field3[64];
	int i;
	int type;

	memset(payload, 0, sizeof(payload));
	memset(field2, 0, sizeof(field2));
	memset(field3, 0, sizeof(field3));

	if (input[0] == '/') {
	// We have a command, start parsing
		for (i = 0; (input[i] != ' ' && input[i] != '\0') && (i < 7); i++) {
			command[i] = input[i];
		}

		// We end on a space, so advance to next real char and null terminate
		command[i++] = '\0';

		// Copy the second argument into field two
		strlcpy(field2, &input[i], sizeof(field2));
		

		if (strcmp(command, "exit") == 0) {
			type = 1;
			memset(field2, 0, sizeof(field2));
			memset(field3, 0, sizeof(field3));

		} else if (strcmp(command, "join") == 0) {
			type = 2;
			memset(field3, 0, sizeof(field3));

		} else if (strcmp(command, "leave") == 0) {
			type = 3;
			memset(field3, 0, sizeof(field3));

		} else if (strcmp(command, "list") == 0) {
			type = 5;
			memset(field2, 0, sizeof(field2));
			memset(field3, 0, sizeof(field3));

		} else if (strcmp(command, "who") == 0) {
			type = 6;
			memset(field3, 0, sizeof(field3));

		} else if (strcmp(command, "switch") == 0) {
			// No type, all client side, but counts as a keep alive
			type = 7;
			strlcpy(channel, field2, sizeof(channel));
			memset(field2, 0, sizeof(field2));
			memset(field3, 0, sizeof(field3));
		} else {
			// Bad command
			type = -1;
		}


	} else {
		type = 4;
		strlcpy(field2, channel, sizeof(field2));
		strlcpy(field3, input, sizeof(field3));
		// Not a command, make a say request
	}

	if (sizeof(int) != 4) {
		// This would be a problem, bail
		type = -1;
	}

	// Set the type, second field, and third field
	memcpy(payload, &type, sizeof(int));
	memcpy(&payload[4], field2, sizeof(field2));
	memcpy(&payload[36], field3, sizeof(field3));

	return;
}

void client_login(int socketfd, struct addrinfo *p, char *nick) {
	int i = 0;
	unsigned int size;
	char payload[36]; // 4 bytes for type, 32 for nick

	memset(payload, 0, sizeof(payload));
	memcpy(payload, &i, sizeof(i));
	memcpy(&payload[4], nick, sizeof(payload)-4);

	size = payload_size(payload);

	if ((i = sendto(socketfd, payload, size, 0,
					p->ai_addr, p->ai_addrlen)) == -1) {
		perror("client: sendto failed\n");
	}

#if DEBUG > 0
	printf("DEBUG: sent %d bytes, login\n", i);
#endif

}

unsigned int payload_size(char *payload) {
	int i;

	memcpy(&i, payload, sizeof(i));

	if (i == -1) 
		return 0;

	if (i == 1 || i == 5 || i == 7)
		return 4;

	if (i == 0 || i == 2 || i == 3 || i == 6)
		return 36;

	if (i == 4)
		return 100;

	// Shouldnt be reached, if it is return error state
	return 0;
}

// Taken directly from 
// http://cc.byexamples.com/2007/04/08/non-blocking-user-input-in-loop-without-ncurses/
int kbhit() 
{
    struct timeval tv;
    fd_set fds;
    tv.tv_sec = 0;
    tv.tv_usec = 0;
    FD_ZERO(&fds);
    FD_SET(STDIN_FILENO, &fds); //STDIN_FILENO is 0
    select(STDIN_FILENO+1, &fds, NULL, NULL, &tv);
    return FD_ISSET(STDIN_FILENO, &fds);
}


