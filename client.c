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


#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#include "raw.h"
#include "client.h"

#define DEBUG 0
#define CHANNEL_SIZE 32
#define PAYLOAD_SIZE 100
#define DGRAM_SIZE 1024

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
		}

		if (connect(socketfd, p->ai_addr, p->ai_addrlen) == -1) {
			close(socketfd);
			perror("client: connect");
			continue;
		}

		break;
	}

	if (p == NULL) {
		fprintf(stderr, "client: failed to make socket\n");
		return 3;
	}


#if DEBUG > 1
	printf("DEBUG: socket/connect worked\n");
#endif


	client_login(socketfd, nick);

	raw_mode();
	status = ui_loop(socketfd);

	printf("\n");

	// Clean Up
	freeaddrinfo(servinfo);
	close(socketfd);
	cooked_mode();


	return status; 
}

int ui_loop(int socketfd) {
	char input[65], buf[65]; //64 bytes we can send, plus newline
	char payload[PAYLOAD_SIZE]; 
	char channel[CHANNEL_SIZE] = "Common";
	char dgram[DGRAM_SIZE];
	int i, num, run = 1, process = 0, num_recv = 0;
	unsigned int j;
	time_t t;

	fcntl(STDIN_FILENO, F_SETFL, O_NONBLOCK);
	fcntl(socketfd, F_SETFL, O_NONBLOCK);

	memset(input, 0, sizeof(input));
	memset(buf, 0, sizeof(buf));

	t = time(NULL);

	printf(">");

	while (run != 0) {
		// Get Keyboard input! Hurray!
		num = read(STDIN_FILENO, buf, sizeof(buf));
		// If were longer than input, it will truncate...

		if (num > -1) {
			for (j = 0; j < strlen(input); j++)
				printf("\b");
			strlcat(input, buf, sizeof(input));
			printf("%s", input);
			fflush(stdin);
		}

		while (num > -1) {
			if (buf[num] == '\n') {
				process = 1;
				// Blast newlines. Note that if you enter text like 'foo\nbar' 
				// faster than 50k usec, this will probably do bad things. However,
				// testing this i found it to be twice the keyboard repeat rate, so
				// so we should be fine
				input[strlen(input) -1] = '\0';

				// do we loop again?
				run = strcmp(input, "/exit");

				break;
			}
			num--;
		}

		// Ok, now that we send everything, lets check if we have anything
		// to recieve. An important note here, Im just allocating a meg to
		// recieve whatever the server sends instead of malloc'ing the exact
		// ammount needed. Just allocating a meg at the start of the program
		// will probably be faster than going back to the heap every time we
		// get a packet.
		if (num_recv <= 0)
			num_recv = recv(socketfd, dgram, DGRAM_SIZE, 0);
		else {
			i = recv(socketfd, &dgram[num_recv], 
					DGRAM_SIZE - num_recv, 0);
			num_recv += i;
		}

		// If we've gotten fewer than 4 bytes, we cant even tell the type so bail
		if (num_recv > 4 && check_size(dgram) == num_recv) {
				display(dgram, input);
				// Rest
				num_recv = 0;
				memset(dgram, 0, sizeof(dgram));
		}

		if (process) {
			client_prepare(input, payload, channel);

			i = payload_size(payload);

			if (!i) {
				// If payload size is 0, something went wrong.
#if DEBUG > 0
				printf("DEBUG: error in payload, size 0");
#endif
				// Reset.
				memset(input, 0, sizeof(input));
				memset(buf, 0, sizeof(buf));
				process = 0;
				continue;
			}

			if ((num = send(socketfd, payload, i, 0)) == -1) {
				perror("client: sendto failed\n");
				return 4;
			}

			// We sent data, update time.
			t = time(NULL);

#if DEBUG > 0
			printf("DEBUG: sent %d bytes\n", num);
#endif

			// This breaks my nice clean presentation and processing
			// abstraction, but thanks to terminals functioning the way
			// they do its required.
			printf("\b");
			for (j = 0; j < strlen(input); j++)
				printf("\b");

			// Reset.
			memset(buf, 0, sizeof(buf));
			memset(input, 0, sizeof(input));
			process = 0;
		}

		if ((t + 60) < time(NULL)) {
			client_keepalive(socketfd);
			t = time(NULL);
		}

		fflush(stdin);
		usleep(5000);
	} 

	return 0;
}

void client_prepare(char *input, char *payload, char *channel) {
	char command[7];
	char field2[32], field3[64];
	int i;
	int type;

	memset(payload, 0, PAYLOAD_SIZE);
	memset(field2, 0, sizeof(field2));
	memset(field3, 0, sizeof(field3));

	if (input[0] == '/') {
		// We have a command, start parsing
		for (i = 0; (input[i+1] != ' ' && input[i+1] != '\0') && (i < 7); i++)
			command[i] = input[i+1];

		// We end on a space, so advance to next real char and null terminate
		command[i++] = '\0';
		i++;

		// Copy the second argument into field two
		strlcpy(field2, &input[i], sizeof(field2));
		

		if (strcmp(command, "exit") == 0) {
			type = 1;
		} else if (strcmp(command, "join") == 0) {
			type = 2;
			strlcpy(channel, field2, CHANNEL_SIZE);
		} else if (strcmp(command, "leave") == 0) {
			type = 3;
			strlcpy(channel, "Common", CHANNEL_SIZE);
		} else if (strcmp(command, "list") == 0) {
			type = 5;
		} else if (strcmp(command, "who") == 0) {
			type = 6;
		} else if (strcmp(command, "switch") == 0) {
			// No type, all client side, but counts as a keep alive
			type = 7;
			strlcpy(channel, field2, CHANNEL_SIZE);
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

void client_login(int socketfd, char *nick) {
	int i = 0, size;
	char payload[36], channel[7] = "Common"; // 4 bytes for type, 32 for nick

	memset(payload, 0, sizeof(payload));
	memcpy(payload, &i, sizeof(i));
	memcpy(&payload[4], nick, sizeof(payload)-4);

	size = payload_size(payload);

	if ((i = send(socketfd, payload, size, 0)) == -1) {
		perror("client: send failed\n");
	}

#if DEBUG > 0
	printf("DEBUG: sent %d bytes, login\n", i);
#endif


	memset(payload, 0, sizeof(payload));
	i = 2;
	memcpy(payload, &i, sizeof(i));
	memcpy(&payload[4], channel, sizeof(payload)-4);


	size = payload_size(payload);

	if ((i = send(socketfd, payload, size, 0)) == -1) {
		perror("client: send failed\n");
	}

#if DEBUG > 0
	printf("DEBUG: sent %d bytes, join\n", i);
#endif

}

void client_keepalive(int socketfd) {
	char payload[4];
	int i, size;

	memset(payload, 0, sizeof(payload));
	i = 7;
	memcpy(payload, &i, sizeof(i));

	size = payload_size(payload);

	if ((i = send(socketfd, payload, size, 0)) == -1) {
		perror("client: send failed\n");
	}

#if DEBUG > 0
	printf("DEBUG: sent %d bytes, keepalive\n", i);
#endif

}

int check_size(char *dgram) {
	int i, j;

	memcpy(&i, dgram, sizeof(i));

	if (i == 0)
		return 4+32+32+64;
	if (i == 3)
		return 4+64;

	if (i == 1 || i == 2) {
		memcpy(&j, &dgram[sizeof(i)], sizeof(j));
		return 4 + 4 + j * 32;
	}

	// Error state
	return -1;
}

void display(char *dgram, char *input) {
	int i, j, k;
	unsigned int m;

	memcpy(&i, dgram, sizeof(i));

	for (m = 0; m < strlen(input) + 1; m++)
		printf("\b");

	switch (i) {
		case 0:
			printf("[%s][%s]: %s", &dgram[4], 
					&dgram[4+32], &dgram[4+32+32]);

			break;
		case 1:
			memcpy(&j, &dgram[sizeof(i)], sizeof(j));
			printf("Existing channels:\n  ");
			for (k = 0; k < j; k++) 
				printf("%s ", &dgram[4+4+k*sizeof(j)]);

			break;
		case 2:
			memcpy(&j, &dgram[sizeof(i)], sizeof(j));
			printf("Users on channel %s:\n  ", &dgram[4+4]);
			for (k = 0; k < j; k++) 
				printf("%s ", &dgram[4+4+32+k*sizeof(j)]);

			break;
		case 3:
			break;
		default:
			break;
	}

	printf("\n>%s", input);
}
