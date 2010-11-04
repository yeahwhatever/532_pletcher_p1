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

	status = listen_loop(socketfd);

	freeaddrinfo(servinfo);

	return 0;
}

int listen_loop(int socketfd) {
	struct sockaddr_storage from;
	int num;
	socklen_t fromlen;
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
		while (num >= 4 && payload_size(buf) != num)
			if ((num = recvfrom(socketfd, &buf[num], sizeof(buf) - num, 0, 
							(struct sockaddr *)&from, &fromlen)) == -1) 
				perror("server: recvfrom");

		if (num >= 4)
			parse_dgram(socketfd, fromlen, buf, from, &clist, &ulist);		
	}
}

void parse_dgram(int socketfd, socklen_t fromlen, char *payload, struct sockaddr_storage from, 
		channel **clist, user **ulist) {
	int type;

	memcpy(&type, payload, sizeof(type));

	switch (type) {
		case 0:
			login(payload, from, ulist);
			break;
		case 1:
			logout(from, ulist, clist);
			break;
		case 2:
			join(payload, from, ulist, clist);
			break;
		case 3:
			leave(payload, from, ulist, clist);
			break;
		case 4:
			say(socketfd, fromlen, payload, from, ulist, clist);
			break;
		case 5:
			list(socketfd, fromlen, from, clist);
			break;
		case 6:
			who(socketfd, fromlen, from, payload, clist);
			break;
			/*
		case 7:
			keepalive(payload, ulist, clist);
			break;
		default:
			//error
			*/
	}
}

void login(char *payload, struct sockaddr_storage from, 
		user **u_list) {
	user *uhead, *ulist;
	//channel *chead, *clist;
	//user *uptr;

	ulist = *u_list;
	uhead = *u_list;
	//clist = *c_list;
	//chead = *c_list;

	if (ulist) {
		while (ulist->next != NULL)
			ulist = ulist->next;

		ulist->next = xmalloc(sizeof(user));
		ulist = ulist->next;
	} else {
		ulist = xmalloc(sizeof(user));
		uhead = ulist;
	}

	// Put the username in the userlist
	strlcpy(ulist->name, &payload[4], sizeof(ulist->name));
	ulist->address = from;
	// Set user time
	ulist->t = time(NULL);
	// Create a channel list for the user. Note we won't use the user_list
	// field here. Set the channel name to Common
	ulist->channel_list = NULL;
	/*
	ulist->channel_list = xmalloc(sizeof(channel));
	ulist->channel_list->user_list = NULL;
	ulist->channel_list->next = NULL;
	*/
	//strlcpy(ulist->channel_list->name, "Common", sizeof(ulist->name));
	ulist->next = NULL;

	/*
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
		chead = clist;
	}

	uptr = clist->user_list;
	while (uptr->next != NULL)
		uptr = uptr->next;

	strlcpy(uptr->name, &payload[4], sizeof(ulist->name));
	uptr->address = from;
	uptr->t = NULL;
	uptr->channel_list = NULL;
	uptr->next = NULL;

	*c_list = chead;
	*/
	*u_list = uhead;
}

void logout(struct sockaddr_storage from,
		user **u_list, channel **c_list) {
	user *ulist, *uptr;
	channel *chead, *clist, *cptr;

	ulist = *u_list;
	clist = *c_list;
	chead = *c_list;
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

	if (!clist)
		*c_list = NULL;

	*c_list = chead;
}

void user_remove(user **u_list, struct sockaddr_storage from) {
	user *ulist, *uptr;
	channel *cptr;

	ulist = *u_list;
	uptr = NULL;
	cptr = NULL;

	while (ulist) 
		if (!sockaddr_storage_equal(&from, &(ulist->address)))
			break;
		else {
			uptr = ulist;
			ulist = ulist->next;
		}

	// We have a match!

	// If first
	if (ulist && !uptr) {
		// Need to free channels
		cptr = ulist->channel_list;
		while (cptr) {
			cptr = cptr->next;
			free(cptr);
		}
		// Free ulist, set old 2nd to head
		*u_list = ulist->next;
		free(ulist);
		// If in the middle, or last
	} else if (ulist) {
		// Clear channels
		cptr = ulist->channel_list;
		while (cptr) {
			cptr = cptr->next;
			free(cptr);
		}
		// uptr points to prev, so we want to skip to next
		uptr->next = ulist->next;
		free(ulist);
	}
}

int sockaddr_storage_equal(struct sockaddr_storage *f1,
		struct sockaddr_storage *f2) {
	struct sockaddr_in sa_1, sa_2;
	struct sockaddr_in6 sa6_1, sa6_2;
	// IPv44
	if (f1->ss_family == AF_INET && f2->ss_family == AF_INET) {
		sa_1 = *((struct sockaddr_in *)f1);
		sa_2 = *((struct sockaddr_in *)f2);

		if (sa_1.sin_port != sa_2.sin_port || 
				sa_1.sin_addr.s_addr != sa_2.sin_addr.s_addr)
			return 1;
		// Not a match, skip
		// IPv6
	} else if (f1->ss_family == AF_INET6 && 
			f2->ss_family == AF_INET6) {
		sa6_1 = *((struct sockaddr_in6 *)f1);
		sa6_2 = *((struct sockaddr_in6 *)f2);

		if (sa6_1.sin6_port != sa6_2.sin6_port || 
				memcmp(sa6_1.sin6_addr.s6_addr, sa6_2.sin6_addr.s6_addr,
					sizeof(sa6_1.sin6_addr.s6_addr))) {
			// Not a match, skip
			return 1;
		}
	} else
		// Also not a match.
		return -1;

	// Match!
	return 0;
}

void join(char *payload, struct sockaddr_storage from, 
		user **u_list, channel **c_list) {
	channel *clist, *new;
	user *ulist, *uptr, *uptr2;

	clist = *c_list;
	ulist = *u_list;

	if (!ulist)
		return;

	// Get the user, update its time, and store it for later use
	uptr = user_lookup(&from, ulist);
	if (!uptr)
		return;
	uptr->t = time(NULL);

	new = uptr->channel_list;
	if (new) {
		while (new->next)
			new = new->next;
		new->next = xmalloc(sizeof(channel));
		new = new->next;
		strlcpy(new->name, &payload[4], sizeof(new->name));
		new->next = NULL;
		new->user_list = NULL;
	} else {
		uptr->channel_list = xmalloc(sizeof(channel));
		new = uptr->channel_list;

		strlcpy(new->name, &payload[4], sizeof(new->name));
		new->next = NULL;
		new->user_list = NULL;
	}

	// Channel list already exists.
	while (clist) {
		if (!strcmp(clist->name, &payload[4])) {
			uptr2 = clist->user_list;
			while (uptr2->next) {
				uptr2 = uptr2->next;
			}
			uptr2->next = xmalloc(sizeof(user));
			uptr2 = uptr2->next;
			memcpy(uptr2, uptr, sizeof(user));
			break;
		} else
			clist = clist->next;
	}

	// Doesn't exist.
	if (!clist) {
		new = xmalloc(sizeof(channel));

		// Copy in its name, and create a userlist
		strlcpy(new->name, &payload[4], sizeof(new->name));
		new->user_list = xmalloc(sizeof(user));

		// Looup the user, easier to copy from userlist than to fill in
		// from scratch (would need to look up username anyways...)
		memcpy(new->user_list, uptr, sizeof(user));
		// Only one user in the channel, so set next to null
		new->user_list->next = NULL;
		// Going to be the last channel, set to null...
		new->next = NULL;
		if (*c_list) {
			clist = *c_list;
			while (clist->next) 
				clist = clist->next;

			// Add it!
			clist->next = new;
		// Otherwise, its the first channel. Start our channel list
		} else
			*c_list = new;
	}

}
	
user* user_lookup(struct sockaddr_storage *from, user *ulist) {
	if (!ulist)
		return NULL;

	while (ulist) {
		if (!sockaddr_storage_equal(from, &(ulist->address)))
			return ulist;
		ulist = ulist->next;
	}

	return NULL;
}

void leave(char *payload, struct sockaddr_storage from,
		user **u_list, channel **c_list) {
	user *uptr;
	channel *cptr, *cptr2 = NULL;

	// Remove the channel from the user's list
	uptr = user_lookup(&from, *u_list);
	
	if (uptr) {
		uptr->t = time(NULL);
		cptr = uptr->channel_list;
		while (cptr) {
			if (!strcmp(cptr->name, &payload[4])) {
				// First
				if (!cptr2) {
					uptr->channel_list = cptr->next;
					free(cptr);
				// Middle or last
				} else {
					cptr2->next = cptr->next;
					free(cptr);
				}
				break;
			}
			cptr2 = cptr;
			cptr = cptr->next;
		}
	}

	// Remove the user from channel from the channel list
	cptr = *c_list;
	cptr2 = NULL;
	while (cptr) {
		if (!strcmp(cptr->name, &payload[4])) {
			user_remove(&(cptr->user_list), from);
			// Empty list, gotta kill the channel
			if (!cptr->user_list) {
				// Damn, were the first channel
				if (!cptr2) {
					cptr2 = cptr->next;
					free(cptr2);
					*c_list = cptr2;
				// Middle or last
				} else {
					cptr2->next = cptr->next;
					free(cptr);
				}
			}
			break;
		}
		cptr2 = cptr;
		cptr = cptr->next;
	}
}

void say(int socketfd, socklen_t fromlen, char *payload, 
		struct sockaddr_storage from, user **u_list, channel **c_list) {
	user *uptr;
	char name[32], buf[4+32+32+64];
	channel *cptr;
	int num;
	struct sockaddr_in6 sa6;

	uptr = user_lookup(&from, *u_list);
	
	memset(name, 0, sizeof(name));
	memset(buf, 0, sizeof(buf));

	if (uptr) {
		uptr->t = time(NULL);
		strlcpy(name, uptr->name, sizeof(name));
	}

	cptr = *c_list;
	while (cptr) {
		if (!strcmp(cptr->name, &payload[4])) 
			break;
		cptr = cptr->next;
	}

	if (!cptr)
		return;

	// Make the buffer!
	num = 0;
	memcpy(buf, &num, 4);
	memcpy(&buf[4], &(cptr->name), sizeof(cptr->name));
	memcpy(&buf[36], name, sizeof(name));
	memcpy(&buf[68], &payload[36], sizeof(buf)-68);

	uptr = cptr->user_list;

	while (uptr) {

		sa6 = *((struct sockaddr_in6 *)&(uptr->address));
		
		if ((num = sendto(socketfd, buf, sizeof(buf), 0, 
				(struct sockaddr *)&(uptr->address), 
				fromlen)) == -1) {
			perror("server: sendto");
		}

		uptr = uptr->next;
	}

}

void list(int socketfd, socklen_t fromlen,
		struct sockaddr_storage from, channel **c_list) {
	channel *cptr;
	char *buf;
	int num, i = 0;


	cptr = *c_list;
	while (cptr) {
		i++;
		cptr = cptr->next;
	}
	cptr = *c_list;

	buf = xmalloc(4+4+i*32);
	
	memset(buf, 0, 4+4+i*32);

	memcpy(&buf[4], &i, sizeof(int));
	i = 1;
	memcpy(buf, &i, sizeof(int));

	i = 0;
	while (cptr) {
		memcpy(&buf[8+i*32], cptr->name, sizeof(cptr->name));
		i++;
		cptr = cptr->next;
	}

	if ((num = sendto(socketfd, buf, 4+4+i*32, 0, 
					(struct sockaddr *)&from, 
					fromlen)) == -1) {
		perror("server: sendto");
	}

	free(buf);
}

void who(int socketfd, socklen_t fromlen, struct sockaddr_storage from, 
		char *payload, channel **c_list) {
	int i = 0, num;

	channel *cptr;
	user *uptr;
	char *buf;

	cptr = *c_list;

	while (cptr) {
		if (!strcmp(cptr->name, &payload[4])) 
			break;
		cptr = cptr->next;
	}

	if (!cptr)
		return;

	uptr = cptr->user_list;

	while (uptr) {
		i++;
		uptr = uptr->next;
	}

	buf = xmalloc(4+4+32+i*32);

	memset(buf, 0, 4+4+i*32);
	memcpy(&buf[4], &i, sizeof(i));
	i = 2;
	memcpy(buf, &i, sizeof(i));
	i = 0;
	memcpy(&buf[8], cptr->name, sizeof(cptr->name));

	uptr = cptr->user_list;
	while (uptr) {
		memcpy(&buf[40+i*32], uptr->name, sizeof(uptr->name));
		i++;
		uptr = uptr->next;
	}

	if ((num = sendto(socketfd, buf, 40+i*32, 0, 
					(struct sockaddr *)&from, 
					fromlen)) == -1) {
		perror("server: sendto");
	}

	free(buf);
}
