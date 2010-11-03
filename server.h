#ifndef SERVER_H
#define SERVER_H

typedef struct {
	char name[32];
	struct user *user_list;
	struct channel *next;
} channel;

typedef struct {
	char name[32];
	time_t t;
	struct channel *channel_list;
	struct user *next;
} user;

int listen_loop(int socketfd);
void parse_dgram(char *payload);

#endif
