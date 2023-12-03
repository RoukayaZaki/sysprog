#include "chat.h"
#include "chat_server.h"
#include <sys/socket.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

struct chat_peer {
	/** Client's socket. To read/write messages. */
	int socket;
	/** Output buffer. */
	struct chat_message *outbox;
	/* ... */
	/* PUT HERE OTHER MEMBERS */
	int outbox_size;
};

struct chat_server {
	/** Listening socket. To accept new clients. */
	int socket;
	/** Array of peers. */
	struct chat_peer *peers[1024];
	/* ... */
	/* PUT HERE OTHER MEMBERS */
	int peers_size;
};

struct chat_server *
chat_server_new(void)
{
	struct chat_server *server = calloc(1, sizeof(*server));
	server->socket = -1;

	/* IMPLEMENT THIS FUNCTION */
	server->peers_size = 0;
	return server;
}

void
chat_server_delete(struct chat_server *server)
{
	if (server->socket >= 0)
		close(server->socket);

	/* IMPLEMENT THIS FUNCTION */

	free(server);
}

int
chat_server_listen(struct chat_server *server, uint16_t port)
{
	struct sockaddr_in addr;
	memset(&addr, 0, sizeof(addr));
	addr.sin_port = htons(port);
	/* Listen on all IPs of this machine. */
	addr.sin_addr.s_addr = htonl(INADDR_ANY);

	/*
	 * 1) Create a server socket (function socket()).
	 * 2) Bind the server socket to addr (function bind()).
	 * 3) Listen the server socket (function listen()).
	 * 4) Create epoll/kqueue if needed.
	 */
	/* IMPLEMENT THIS FUNCTION */
	if(server->socket != -1)
	{
		return CHAT_ERR_ALREADY_STARTED;
	}
	server->socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (server->socket == -1) {
		return CHAT_ERR_SYS;
	}
	fcntl(server->socket, F_SETFL, O_NONBLOCK | fcntl(server->socket, F_GETFL, 0));
	if (bind(server->socket, (struct sockaddr *) &addr, sizeof(addr)) == -1)
	{
		return CHAT_ERR_PORT_BUSY;
	}

	if (listen(server->socket, 1024) == -1) {
		return CHAT_ERR_SYS;
	}

	// TODO: create epoll;
	return 0;
}

struct chat_message *
chat_server_pop_next(struct chat_server *server)
{
	/* IMPLEMENT THIS FUNCTION */
	(void)server;
	return NULL;
}

int
chat_server_update(struct chat_server *server, double timeout)
{
	/*
	 * 1) Wait on epoll/kqueue/poll for update on any socket.
	 * 2) Handle the update.
	 * 2.1) If the update was on listen-socket, then you probably need to
	 *     call accept() on it - a new client wants to join.
	 * 2.2) If the update was on a client-socket, then you might want to
	 *     read/write on it.
	 */
	// Error handling
	if(server->socket == -1)
	{
		return CHAT_ERR_NOT_STARTED;
	}
	(void) timeout;
	// TODO: actual implemantation
	return CHAT_ERR_NOT_IMPLEMENTED;
}

int
chat_server_get_descriptor(const struct chat_server *server)
{
#if NEED_SERVER_FEED
	/* IMPLEMENT THIS FUNCTION if want +5 points. */

	/*
	 * Server has multiple sockets - own and from connected clients. Hence
	 * you can't return a socket here. But if you are using epoll/kqueue,
	 * then you can return their descriptor. These descriptors can be polled
	 * just like sockets and will return an event when any of their owned
	 * descriptors has any events.
	 *
	 * For example, assume you created an epoll descriptor and added to
	 * there a listen-socket and a few client-sockets. Now if you will call
	 * poll() on the epoll's descriptor, then on return from poll() you can
	 * be sure epoll_wait() can return something useful for some of those
	 * sockets.
	 */
#endif
	(void)server;
	return -1;
}

int
chat_server_get_socket(const struct chat_server *server)
{
	return server->socket;
}

int
chat_server_get_events(const struct chat_server *server)
{

	/*
	 * IMPLEMENT THIS FUNCTION - add OUTPUT event if has non-empty output
	 * buffer in any of the client-sockets.
	 */
	for(int i = 0; i < server->peers_size; i++)
	{
		if(server->peers[i]->outbox_size > 0)
		{
			return CHAT_EVENT_OUTPUT | CHAT_EVENT_INPUT;
		}
	}
	if(server->socket == -1) return 0;
	return CHAT_EVENT_INPUT;
}

int
chat_server_feed(struct chat_server *server, const char *msg, uint32_t msg_size)
{
#if NEED_SERVER_FEED
	/* IMPLEMENT THIS FUNCTION if want +5 points. */
#endif
	(void)server;
	(void)msg;
	(void)msg_size;
	return CHAT_ERR_NOT_IMPLEMENTED;
}
