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
	int sent;
};

struct chat_server
{
	/** Listening socket. To accept new clients. */
	int socket;
	/** Array of peers. */
	struct chat_peer peers[1024];
	/* ... */
	/* PUT HERE OTHER MEMBERS */
	struct chat_message *to_be_sent;
	int capacity;
	int sent;
	int recieved;
	int peers_size;
	int epollfd;
	struct epoll_event event;
};

struct chat_server *
chat_server_new(void)
{
	struct chat_server *server = calloc(1, sizeof(*server));
	server->socket = -1;

	server->peers_size = 0;
	server->sent = 0;
	server->recieved = 0;
	server->capacity = 2048;
	server->to_be_sent = malloc(sizeof(struct chat_message) * server->capacity);
	return server;
}

void chat_server_delete(struct chat_server *server)
{
	if (server->socket >= 0)
		close(server->socket);

	for (int i = 0; i < server->recieved; i++)
	{
		free(server->to_be_sent[server->recieved].data);
	}
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
	/* IMPLEMENT THIS FUNCTION */
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

	struct chat_message *msg = malloc(sizeof(struct chat_message));
	msg->data = strdup(server->to_be_sent[0].data);
	msg->size = server->to_be_sent[0].size;
	free(server->to_be_sent[0].data);
	for (int j = 1; j < server->recieved; j++)
	{
		server->to_be_sent[j - 1] = server->to_be_sent[j];
	}
	// free(server->to_be_sent[server->recieved].data);
	// server->to_be_sent[]
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
			struct sockaddr_in address;
			socklen_t length = sizeof(address);
			// int new_client_fd = accept(server->socket, (struct sockaddr *)&address, &length);
			int new_client_fd = accept(server->socket, NULL, NULL);
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
				// todo: handle client addition failure, delete close free
				printf("yea adding client failed\n");
				return CHAT_ERR_SYS;
			}

			server->peers[server->peers_size].socket = new_client_fd;
			server->peers[server->peers_size].outbox_size = 0;
			server->peers[server->peers_size].sent = 0;
			server->peers_size++;
			// printf("I got new client #%d\n", server->peers_size);
		}

		// TODO: handle the coming message
		// do_use_fd(events[i].data.fd);
		if (events[i].events & EPOLLIN && events[i].data.fd != server->socket)
		{
			// printf("Incoming server\n");

			char *buffer = calloc(2048, sizeof(char));
			int total_bytes_read = 0, capacity = 2048;
			while (true)
			{
				int bytes_read = recv(events[i].data.fd, buffer + total_bytes_read, capacity - total_bytes_read, MSG_DONTWAIT);

				if (bytes_read <= 0)
					break;
				total_bytes_read += bytes_read;
				if (total_bytes_read + 1 >= capacity)
				{
					capacity *= 2;
					buffer = realloc(buffer, capacity);
				}
			}
			// printf("Bytes recieved: %ld, buffer: %s\n\n\n", total_bytes_read, buffer);
			// Connection closed, delete peer
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
						close(server->peers[j].socket);
						closed = true;
						epoll_ctl(server->epollfd, EPOLL_CTL_DEL, events[i].data.fd, &events[i]);
					}
				}
				server->peers_size--;
				// break;
			}

			// server->to_be_sent[server->recieved].data = realloc(server->to_be_sent[server->recieved].data, bytes_read);
			int start = 0;
			for (int idx = 0; idx < total_bytes_read; idx++)
			{
				// printf("Char: %c\n", buffer[idx]);
				if(buffer[idx] == '\0' && start == idx)
				{
					// printf("\n\n\n----------Warning------------\n\n\n");
					start++;
				}
				if (buffer[idx] == '\n')
				{
					int size = idx - start;
					// printf("Let's put message in server of size: %d with starting char: %c\n", size, buffer[start]);
					server->to_be_sent[server->recieved].data = calloc(size + 1, sizeof(char));
					memcpy(server->to_be_sent[server->recieved].data, buffer + start, size);
					server->to_be_sent[server->recieved].size = size;
					server->to_be_sent[server->recieved].data[size] = '\0';
					// printf("Server Message: %s\n\n\n", server->to_be_sent[server->recieved]);
					server->recieved++;
					if(server->recieved >= server->capacity)
					{
						server->capacity *= 2;
						server->to_be_sent = realloc(server->to_be_sent, sizeof(struct chat_message) * server->capacity);
					}
					start = idx + 1;
				}
			}
			// printf("Message recieved: %s\n", server->to_be_sent[server->recieved-1].data);
			for (int j = 0; j < server->peers_size; j++)
			{
				// printf("Client #%d\n", j);
				if (events[i].data.fd == server->peers[j].socket)
					continue;

				struct epoll_event new_event;
				new_event.data.fd = server->peers[j].socket;
				new_event.events = EPOLLIN | EPOLLET | EPOLLOUT;
				// printf("sending to client: %d\n", j);
				if (epoll_ctl(server->epollfd, EPOLL_CTL_MOD, server->peers[j].socket, &new_event) < 0)
				{
					perror("Modify");
					free(buffer);
					return CHAT_ERR_SYS;
				}
				server->peers[j].outbox = realloc(server->peers[j].outbox, server->peers[j].outbox_size + total_bytes_read + 1);
				memcpy(server->peers[j].outbox + server->peers[j].outbox_size, buffer, total_bytes_read);
				server->peers[j].outbox_size += total_bytes_read;
				// if(server->peers[j].outbox[(server->peers[j].outbox_size - 1)] == '\0')
				// 	printf("%s\n", server->peers[j].outbox);
			}
			free(buffer);
			// printf("Server recieved: %d messages\n", server->recieved);
		}
		if (events[i].events & EPOLLOUT)
		{
			// printf("Outgoing Server\n");
			
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

				int total_bytes_sent = 0;
				
				while ((int)total_bytes_sent < server->peers[idx].outbox_size)
				{
					// printf("going: %s\n", server->peers[idx].outbox);
					int bytes_sent = send(server->peers[idx].socket, server->peers[idx].outbox + total_bytes_sent, server->peers[idx].outbox_size - total_bytes_sent, 0);
					if (bytes_sent <= -1)
					{
						perror("send");
						break;
						// return CHAT_ERR_SYS;
					}
					if (bytes_sent == 0)
					{
						// close(client->socket);
						// return 0;
						break;
					}
					// printf("Bytes sending: %ld Message: %s\n", bytes_sent, client->sent_msgs[client->sent].data);
					total_bytes_sent += bytes_sent;
				}
				// printf("Bytes Sent: %ld, Message sent: %s\n", total_bytes_sent, server->peers[idx].outbox);
				// if (total_bytes_sent < server->peers[idx].outbox_size)
				// {
				// 	free(server->peers[idx].outbox);
				// 	close(server->peers[idx].socket);
				// 	closed = true;
				// 	epoll_ctl(server->epollfd, EPOLL_CTL_DEL, events[i].data.fd, &events[i]);
				// }
				// else
				// {
				// }
					free(server->peers[idx].outbox);
					server->peers[idx].outbox = NULL;
					server->peers[idx].outbox_size = 0;
			}
			events[i].events &= ~EPOLLOUT;
			if(epoll_ctl(server->epollfd, EPOLL_CTL_MOD, events[i].data.fd, &events[i]) < 0)
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
	(void)server;
	return -1;
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
