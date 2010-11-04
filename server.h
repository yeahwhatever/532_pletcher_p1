#ifndef SERVER_H
#define SERVER_H

typedef struct _channel {
	char name[32];
	struct _user *user_list;
	struct _channel *next;
} channel;

typedef struct _user {
	char name[32];
	struct sockaddr_storage address;
	time_t t;
	struct _channel *channel_list;
	struct _user *next;
} user;

int listen_loop(int socketfd);
void parse_dgram(char *payload, struct sockaddr_storage from,
		channel **clist, user **ulist);
void login(char *payload, struct sockaddr_storage from, 
		user **u_list, channel **c_list);
void logout(char *payload, struct sockaddr_storage from,
		user **u_list, channel **c_list);

#endif
