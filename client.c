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
		if ((socketfd = socket(p->ai_family, p->ai_socktype,
			p->ai_protocol)) == -1) {
			perror("client: call to socket failed");
			continue;
		}

		break;
	}

	if (p == NULL) {
		fprintf(stderr, "client: failed to make socket\n");
		return 3;
	}

#if DEBUG > 1
	printf("DEBUG: socket worked\n");
#endif

	status = ui_loop(socketfd, p);

	// Clean Up
	freeaddrinfo(servinfo);
	close(socketfd);

	return 0; // Not reached
}

int ui_loop(int socketfd, struct addrinfo *p) {
	char input[65]; //64 bytes we can send, plus newline
	char payload[4+32+64]; // 32 bit header, 32 byte first field, payload
	int num;

	do {
		printf("> ");
		fgets(input, sizeof(input), stdin);

#if DEBUG > 0
		printf("DEBUG: input |%s|\n", input);
#endif

		payload = client_prepare(input);

		if ((num = sendto(socketfd, payload, strlen(payload), 0,
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

char* client_prepare(char *input) {
	// 32 bit header, 32 byte first field, payload
	char command[7], payload[4+32+64]; 
	int i;
	unsigned int type;

	if (input[0] == '/') {
	// We have a command, start parsing
		for (i = 0; (input[i] != ' ') && (i < 7); i++) {
			command[i] = input[i];
		}

		// We end on a space, so advance to next real char and null terminate
		command[i++] = '\0'; 

		if (strcmp(command, "exit") == 0) {

		} else if (strcmp(command, "join") == 0) {
			type = 2;

		} else if (strcmp(command, "leave") == 0) {
			type = 3;

		} else if (strcmp(command, "list") == 0) {
			type = 5;

		} else if (strcmp(command, "who") == 0) {
			type = 6;

		} else if (strcmp(command, "switch") == 0) {
			// No type, all client side

		}

	} else {
		// Not a command, make a say request
	}

	return payload;
}

void nonblock(int state)
{
    struct termios ttystate;

    //get the terminal state
    tcgetattr(STDIN_FILENO, &ttystate);

    if (state==NB_ENABLE)
    {
        //turn off canonical mode
        ttystate.c_lflag &= ~ICANON;
        //minimum of number input read.
        ttystate.c_cc[VMIN] = 1;
    }
    else if (state==NB_DISABLE)
    {
        //turn on canonical mode
        ttystate.c_lflag |= ICANON;
    }
    //set the terminal attributes.
    tcsetattr(STDIN_FILENO, TCSANOW, &ttystate);

}

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


