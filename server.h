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
void parse_dgram(int socketfd, socklen_t fromlen, char *payload, 
		struct sockaddr_storage from, channel **clist, user **ulist);
void login(char *payload, struct sockaddr_storage from, 
		user **u_list);
void logout(struct sockaddr_storage from, user **u_list, channel **c_list);
int sockaddr_storage_equal(struct sockaddr_storage *f1, 
		struct sockaddr_storage *f2);
void user_remove(user **u_list, struct sockaddr_storage from);
void join(char *payload, struct sockaddr_storage from, 
		user **u_list, channel **c_list);
user* user_lookup(struct sockaddr_storage *from, user *ulist);
void leave(char *payload, struct sockaddr_storage from, 
		user **u_list, channel **c_list);
void say(int socketfd, socklen_t fromlen, char *payload, 
		struct sockaddr_storage from, user **u_list, channel **c_list);

#endif
