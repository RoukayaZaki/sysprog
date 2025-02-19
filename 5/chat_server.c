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
#include <stdbool.h>
#include <errno.h>
#define _GNU_SOURCE

#define MAX_EVENTS 1025
struct chat_peer
{
	/** Client's socket. To read/write messages. */
	int socket;
	/** Output buffer. */
	char *outbox;
	/* ... */
	/* PUT HERE OTHER MEMBERS */
	int outbox_size;
};

struct chat_server
{
	/** Listening socket. To accept new clients. */
	int socket;
	/** Array of peers. */
	struct chat_peer peers[1024];
	/* ... */
	/* PUT HERE OTHER MEMBERS */
	struct chat_message **to_be_sent;
	char *buffer[1024];
	int buff_size[1024];
	int buff_capacity[1024];
	int capacity;
	int recieved;
	int peers_size;
	int epollfd;
	char *last_message;
	int cursor;
	struct epoll_event event;
};

struct chat_server *
chat_server_new(void)
{
	struct chat_server *server = calloc(1, sizeof(*server));
	server->socket = -1;

	server->peers_size = 0;
	server->recieved = 0;
	server->capacity = 2048;
	server->cursor = 0;
	server->last_message = NULL;
	server->to_be_sent = malloc(sizeof(struct chat_message *) * server->capacity);
	for(int i = 0; i < 1024; i++)
	{
		server->buff_capacity[i] = 2048;
		server->buff_size[i] = 0;
		server->buffer[i] = malloc(2048 * sizeof(char));
	}
	return server;
}

void chat_server_delete(struct chat_server *server)
{
	if (server->socket >= 0)
		close(server->socket);

	for (int i = 0; i < server->recieved; i++)
	{
		free(server->to_be_sent[i]->data);
		free(server->to_be_sent[i]);
	}
	free(server->to_be_sent);
	for (int i = 0; i < server->peers_size; i++)
	{
		free(server->peers[i].outbox);
	}
	for(int i = 0; i < 1024; i++)
		free(server->buffer[i]);
	free(server);
}

// reference: Lecture 9
void server_make_fd_nonblocking(int fd)
{
	int old_flags = fcntl(fd, F_GETFL);
	fcntl(fd, F_SETFL, old_flags | O_NONBLOCK);
}
int chat_server_listen(struct chat_server *server, uint16_t port)
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
	if (server->socket != -1)
	{
		return CHAT_ERR_ALREADY_STARTED;
	}
	server->socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (server->socket == -1)
	{
		return CHAT_ERR_SYS;
	}
	server_make_fd_nonblocking(server->socket);
	if (bind(server->socket, (struct sockaddr *)&addr, sizeof(addr)) == -1)
	{
		return CHAT_ERR_PORT_BUSY;
	}

	if (listen(server->socket, 1024) == -1)
	{
		return CHAT_ERR_SYS;
	}

	server->epollfd = epoll_create1(0);
	if (server->epollfd == -1)
	{
		return CHAT_ERR_SYS;
	}

	server->event.events = EPOLLIN | EPOLLET;
	server->event.data.fd = server->socket;
	if (epoll_ctl(server->epollfd, EPOLL_CTL_ADD, server->socket, &server->event) == -1)
	{
		return CHAT_ERR_SYS;
	}
	return 0;
}

struct chat_message *
chat_server_pop_next(struct chat_server *server)
{
	if (server->recieved == 0)
	{
		return NULL;
	}

	struct chat_message *msg = server->to_be_sent[0];

	for (int j = 1; j < server->recieved; j++)
	{
		server->to_be_sent[j - 1] = server->to_be_sent[j];
	}

	server->recieved--;

	return msg;
}

int chat_server_update(struct chat_server *server, double timeout)
{
	/*
	 * 1) Wait on epoll/kqueue/poll for update on any socket.
	 * 2) Handle the update.
	 * 2.1) If the update was on listen-socket, then you probably need to
	 *     call accept() on it - a new client wants to join.
	 * 2.2) If the update was on a client-socket, then you might want to
	 *     read/write on it.
	 */
	if (server->socket == -1)
	{
		return CHAT_ERR_NOT_STARTED;
	}

	// reference: https://man7.org/linux/man-pages/man7/epoll.7.html
	struct epoll_event events[MAX_EVENTS];

	int nfds = epoll_wait(server->epollfd, events, MAX_EVENTS, timeout * 1000.0);
	if (nfds == -1)
	{

		return CHAT_ERR_SYS;
	}
	if (nfds == 0)
	{
		return CHAT_ERR_TIMEOUT;
	}
	for (int i = 0; i < nfds; i++)
	{
		if (events[i].data.fd == server->socket)
		{
			// reference: https://mecha-mind.medium.com/a-non-threaded-chat-server-in-c-53dadab8e8f3
			while (true)
			{
				// struct sockaddr_in address;
				// socklen_t length = sizeof(address);
				int new_client_fd = accept(server->socket, NULL, NULL);
				if (new_client_fd == -1)
				{
					if ((errno == EAGAIN) || (errno == EWOULDBLOCK))
					{
						break;
					}
					else
					{
						perror("accept");
						break;
					}
				}
				if (new_client_fd == -1)
				{

					return CHAT_ERR_SYS;
				}
				server_make_fd_nonblocking(new_client_fd);
				server->event.events = EPOLLIN | EPOLLET;
				server->event.data.fd = new_client_fd;
				if (epoll_ctl(server->epollfd, EPOLL_CTL_ADD, new_client_fd, &server->event) == -1)
				{
					perror("epoll_ctl: new_client_fd");
					return CHAT_ERR_SYS;
				}

				server->peers[server->peers_size].socket = new_client_fd;
				server->peers[server->peers_size].outbox_size = 0;
				server->peers_size++;
			}
		}

		if (events[i].events & EPOLLIN && events[i].data.fd != server->socket)
		{
			int total_bytes_read = 0;
			int peer_idx;
			for(int x = 0; x < server->peers_size; x++)
			{
				if(events[i].data.fd == server->peers[x].socket)
				{
					peer_idx = x;
					break;
				}
			}
			while (true)
			{
				int bytes_read = recv(events[i].data.fd, server->buffer[peer_idx] + server->buff_size[peer_idx], server->buff_capacity[peer_idx] - server->buff_size[peer_idx], MSG_DONTWAIT);

				if (bytes_read <= 0)
					break;
				total_bytes_read += bytes_read;
				server->buff_size[peer_idx] += bytes_read;
				if (server->buff_size[peer_idx] + 1 >= server->buff_capacity[peer_idx])
				{
					server->buff_capacity[peer_idx] *= 2;
					server->buffer[peer_idx] = realloc(server->buffer[peer_idx], server->buff_capacity[peer_idx]);
				}
			}
			if (total_bytes_read == 0)
			{
				bool closed = false;
				for (int j = 0; j < server->peers_size; j++)
				{
					if (closed)
					{
						server->peers[j - 1] = server->peers[j];
					}
					if (server->peers[j].socket == events[i].data.fd)
					{
						free(server->peers[j].outbox);
						server->peers[j].outbox = NULL;
						free(server->buffer[peer_idx]);
						server->buffer[peer_idx] = malloc(2048 * sizeof(char));
						server->buff_size[peer_idx] = 0;
						server->buff_capacity[peer_idx] = 2048;
						close(server->peers[j].socket);
						closed = true;
						epoll_ctl(server->epollfd, EPOLL_CTL_DEL, events[i].data.fd, &events[i]);
					}
				}
				server->peers_size--;
			}
			int start = 0;
			for (int idx = 0; idx < total_bytes_read; idx++)
			{
				if (server->buffer[peer_idx][idx] == '\0' && start == idx)
				{
					start++;
				}
				if (server->buffer[peer_idx][idx] == '\n')
				{
					int size = idx - start;
					struct chat_message *msg = malloc(sizeof(struct chat_message));
					msg->data = calloc(size + 1, sizeof(char));
					memcpy(msg->data, server->buffer[peer_idx] + start, size);
					msg->data[size] = '\0';
					msg->size = size;
					server->to_be_sent[server->recieved] = msg;
					server->recieved++;
					if (server->recieved >= server->capacity)
					{
						server->capacity *= 2;
						server->to_be_sent = realloc(server->to_be_sent, sizeof(struct chat_message *) * server->capacity);
					}
					start = idx + 1;
				}
			}
			for (int j = 0; j < server->peers_size; j++)
			{
				if (events[i].data.fd == server->peers[j].socket)
					continue;

				struct epoll_event new_event;
				new_event.data.fd = server->peers[j].socket;
				new_event.events = EPOLLIN | EPOLLET | EPOLLOUT;
				if (epoll_ctl(server->epollfd, EPOLL_CTL_MOD, server->peers[j].socket, &new_event) < 0)
				{
					perror("Modify");
					// free(buffer);
					return CHAT_ERR_SYS;
				}
				server->peers[j].outbox = realloc(server->peers[j].outbox, server->peers[j].outbox_size + start + 1);
				memcpy(server->peers[j].outbox + server->peers[j].outbox_size, server->buffer[peer_idx], start);
				// server->peers[j].outbox[start] = '\0';
				server->peers[j].outbox_size += start;
			}
			if (start == server->buff_size[peer_idx])
			{
				free(server->buffer[peer_idx]);
				// while(server->buffer == NULL)
				server->buffer[peer_idx] = malloc(2048 * sizeof(char));
				server->buff_capacity[peer_idx] = 2048;
				server->buff_size[peer_idx] = 0;
			}
			else
			{
				int rest = server->buff_size[peer_idx] - start;
				memmove(server->buffer[peer_idx], server->buffer[peer_idx] + start, rest);
				server->buff_size[peer_idx] -= start;
			}
		}
		if (events[i].events & EPOLLOUT)
		{
			bool closed = false;
			for (int idx = 0; idx < server->peers_size; idx++)
			{
				if (server->peers[idx].socket != events[i].data.fd)
				{
					if (closed)
					{
						server->peers[idx - 1] = server->peers[idx];
					}
					continue;
				}
				if(server->peers[idx].outbox[server->peers[idx].outbox_size - 1] != '\0')
				{
					server->peers[idx].outbox = realloc(server->peers[idx].outbox, server->peers[idx].outbox_size + 1);
					server->peers[idx].outbox[server->peers[idx].outbox_size] = '\0';
				}
				int total_bytes_sent = 0;

				while ((int)total_bytes_sent < server->peers[idx].outbox_size)
				{
					int bytes_sent = send(server->peers[idx].socket, server->peers[idx].outbox + total_bytes_sent, server->peers[idx].outbox_size - total_bytes_sent, 0);
					if (bytes_sent <= -1)
					{
						if (errno == EWOULDBLOCK || errno == EAGAIN)
						{

							int rest = server->peers[idx].outbox_size - total_bytes_sent;
							if (rest <= 0)
							{
								break;
							}
							
							memmove(server->peers[idx].outbox, server->peers[idx].outbox + total_bytes_sent, rest);
							server->peers[idx].outbox_size = rest;
						}
						break;
					}
					if (bytes_sent == 0)
					{
						break;
					}
					total_bytes_sent += bytes_sent;
				}
				free(server->peers[idx].outbox);
				server->peers[idx].outbox = NULL;
				server->peers[idx].outbox_size = 0;
			}
			events[i].events &= ~EPOLLOUT;
			if (epoll_ctl(server->epollfd, EPOLL_CTL_MOD, events[i].data.fd, &events[i]) < 0)
			{
				return CHAT_ERR_SYS;
			}
		}
	}

	return 0;
}

int chat_server_get_descriptor(const struct chat_server *server)
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
	return server->epollfd;
}

int chat_server_get_socket(const struct chat_server *server)
{
	return server->socket;
}

int chat_server_get_events(const struct chat_server *server)
{

	/*
	 * IMPLEMENT THIS FUNCTION - add OUTPUT event if has non-empty output
	 * buffer in any of the client-sockets.
	 */
	for (int i = 0; i < server->peers_size; i++)
	{
		if (server->peers[i].outbox_size > 0)
		{
			return CHAT_EVENT_OUTPUT | CHAT_EVENT_INPUT;
		}
	}
	if (server->socket == -1)
		return 0;
	return CHAT_EVENT_INPUT;
}

int chat_server_feed(struct chat_server *server, const char *msg, uint32_t msg_size)
{
#if NEED_SERVER_FEED
	/* IMPLEMENT THIS FUNCTION if want +5 points. */
#endif
	(void)server;
	(void)msg;
	(void)msg_size;
	return CHAT_ERR_NOT_IMPLEMENTED;
}
