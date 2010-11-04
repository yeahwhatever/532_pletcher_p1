/**
 * Joe Pletcher
 * CIS 532 Programming Project 1
 *
 * server.c
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

#include "server.h"
#include "common.h"

#define DEBUG 2

int main(int argc, char *argv[]) {
	int status, socketfd;
	struct addrinfo hints, *servinfo, *p;
	char *host, *port;

	if (argc != 3) {
		printf("Usage: %s <host> <port>\n", argv[0]);
		return 1;
	} else {
		host = argv[1];
		port = argv[2];
	}

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC; // set to AF_INET to force IPv4
	hints.ai_socktype = SOCK_DGRAM;
	hints.ai_flags = AI_PASSIVE; // use my IP

	if ((status = getaddrinfo(NULL, port, &hints, &servinfo)) != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(status));
		return 2;
	}

    // loop through all the results and bind to the first we can
    for(p = servinfo; p != NULL; p = p->ai_next) {
        if ((socketfd = socket(p->ai_family, p->ai_socktype,
                p->ai_protocol)) == -1) {
            perror("server: socket");
            continue;
        }

        if (bind(socketfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(socketfd);
            perror("server: bind");
            continue;
        }

        break;
    }

    if (p == NULL) {
        fprintf(stderr, "listener: failed to bind socket\n");
        return 3;
    }

	freeaddrinfo(servinfo);

	status = listen_loop(socketfd);

	return 0;
}

int listen_loop(int socketfd) {
	struct sockaddr_storage from;
	int fromlen, num;
	char buf[4+32+64]; // Largest message is a say...
	channel *clist;
   	user *ulist;

	ulist = NULL;
	clist = NULL;

	fromlen	= sizeof(struct sockaddr_storage);

	while (1) {
		// If we read fewer than 4 bytes, were probably screwed anyways
		// Im pretty sure this will be rare enough that we don't need to worry
		// about it
		if ((num = recvfrom(socketfd, buf, sizeof(buf), 0, 
			(struct sockaddr *)&from, &fromlen)) == -1) 
			perror("server: recvfrom");

		// Make sure we get all the data from this guy.
		while (num > 4 && payload_size(buf) == num)
			if ((num = recvfrom(socketfd, &buf[num], sizeof(buf) - num, 0, 
							(struct sockaddr *)&from, &fromlen)) == -1) 
				perror("server: recvfrom");

		parse_dgram(buf, from, &clist, &ulist);		
	}
}

void parse_dgram(char *payload, struct sockaddr_storage from, 
		channel **clist, user **ulist) {
	int type;

	memcpy(&type, payload, sizeof(type));

	switch (type) {
		case 0:
			login(payload, from, ulist, clist);
			break;
		case 1:
			logout(from, ulist, clist);
			break;
			/*
		case 2:
			join(payload, ulist, clist);
			break;
		case 3:
			leave(payload, ulist, clist);
			break;
		case 4:
			say(payload, ulist, clist);
			break;
		case 5:
			list(payload, ulist, clist);
			break;
		case 6:
			who(payload, ulist, clist);
			break;
		case 7:
			keepalive(payload, ulist, clist);
			break;
		default:
			//error
			*/
	}
}

void login(char *payload, struct sockaddr_storage from, 
		user **u_list, channel **c_list) {
	user *ulist;
	channel *clist;
	user *uptr;

	ulist = *u_list;
	clist = *c_list;

	if (ulist) {
		while (ulist->next != NULL)
			ulist = ulist->next;

		ulist->next = xmalloc(sizeof(user));
		ulist = ulist->next;
	} else 
		ulist = xmalloc(sizeof(user));

	// Put the username in the userlist
	strlcpy(ulist->name, &payload[4], sizeof(ulist->name));
	ulist->address = from;
	// Set user time
	ulist->t = time(NULL);
	// Create a channel list for the user. Note we won't use the user_list
	// field here. Set the channel name to Common
	ulist->channel_list = xmalloc(sizeof(channel));
	ulist->channel_list->user_list = NULL;
	ulist->channel_list->next = NULL;
	strlcpy(ulist->channel_list->name, "Common", sizeof(ulist->name));
	ulist->next = NULL;

	if (clist) {
		while (clist && strcmp(clist->name, "Common"))
			clist = clist->next;

		if (!clist) {
			fprintf(stderr, "No common channel.\n");
			exit(1);
		}
		
	} else {
		clist = xmalloc(sizeof(channel));
		strlcpy(clist->name, "Common", sizeof(ulist->name));
		clist->user_list = xmalloc(sizeof(user));
		clist->user_list->next = NULL;
		clist->next = NULL;
	}

	uptr = clist->user_list;
	while (uptr->next != NULL)
		uptr = uptr->next;

	strlcpy(uptr->name, &payload[4], sizeof(ulist->name));
	uptr->address = from;
	uptr->t = NULL;
	uptr->channel_list = NULL;
	uptr->next = NULL;

	*c_list = clist;
	*u_list = ulist;
}

void logout(struct sockaddr_storage from,
		user **u_list, channel **c_list) {
	user *ulist, *uptr;
	channel *clist, *cptr;

	ulist = *u_list;
	clist = *c_list;
	uptr = NULL;
	cptr = NULL;

	// Bail if null
	if (!ulist || !clist) {
		fprintf(stderr, "userlist or channel list are null\n");
		return;
	}

	user_remove(u_list, from);

	while (clist) {
		user_remove(&(clist->user_list), from);
		// Userlist is null, channel is empty
		// Note we dont care about channel->user->channel
		if (!clist->user_list) {
			// If first
			if (!cptr) {
				cptr = clist->next;
				free(clist);
				clist = cptr;
				break;
			// Middle or last
			} else {
				cptr->next = clist->next;
				free(clist);
				break;
			}
		}
		cptr = clist;
		clist = clist->next;
	}

	*c_list = clist;
}

void user_remove(user **u_list, struct sockaddr_storage from) {
	struct sockaddr_in sa_1, sa_2;
	struct sockaddr_in6 sa6_1, sa6_2;

	user *ulist, *uptr;
	channel *cptr;

	ulist = *u_list;
	uptr = NULL;
	cptr = NULL;

	while (ulist) {
		// IPv44
		if (from.ss_family == AF_INET && ulist->address.ss_family == AF_INET) {
			sa_1 = *((struct sockaddr_in *)&from);
			sa_2 = *((struct sockaddr_in *)&(ulist->address));

			if (sa_1.sin_port != sa_2.sin_port || 
					sa_1.sin_addr.s_addr != sa_2.sin_addr.s_addr) {
				// Not a match, skip
				uptr = ulist;
				ulist = ulist->next;
				continue;
			}
		// IPv6
		} else if (from.ss_family == AF_INET6 && 
				ulist->address.ss_family == AF_INET6) {
			sa6_1 = *((struct sockaddr_in6 *)&from);
			sa6_2 = *((struct sockaddr_in6 *)&(ulist->address));

			if (sa6_1.sin6_port != sa6_2.sin6_port || 
					!memcmp(sa6_1.sin6_addr.s6_addr, sa6_2.sin6_addr.s6_addr,
					sizeof(sa6_1.sin6_addr.s6_addr))) {
				// Not a match, skip
				uptr = ulist;
				ulist = ulist->next;
				continue;
			}
		} else {
			uptr = ulist;
			ulist = ulist->next;
			continue;

		}

		// We have a match!

		// If first
		if (uptr == NULL) {
			// Store next, this will be new head
			uptr = ulist->next;
			// Need to free channels
			cptr = ulist->channel_list;
			while (cptr) {
				cptr = cptr->next;
				free(cptr);
			}
			// Free ulist, set old 2nd to head
			free(ulist);
			ulist = uptr;
			break;
			// If in the middle, or last
		} else {
			// Clear channels
			cptr = ulist->channel_list;
			while (cptr) {
				cptr = cptr->next;
				free(cptr);
			}
			// uptr points to prev, so we want to skip to next
			uptr->next = ulist->next;
			free(ulist);
			break;
		}
	}

	*u_list = ulist;
}
