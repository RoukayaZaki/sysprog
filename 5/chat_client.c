#include "chat.h"
#include "chat_client.h"
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <sys/epoll.h>
#include <poll.h>
#include <stdbool.h>

#define MAX_EVENTS 1024
struct chat_client
{
	/** Socket connected to the server. */
	int socket;
	/** Array of received messages. */
	struct chat_message recieved_msgs[1024];
	/* ... */
	/** Output buffer. */
	struct chat_message sent_msgs[1024];
	/* ... */
	/* PUT HERE OTHER MEMBERS */
	int sent;
	int to_send;
	int recieved;
	struct pollfd fd;
};

struct chat_client *
chat_client_new(const char *name)
{
	/* Ignore 'name' param if don't want to support it for +5 points. */
	(void)name;

	struct chat_client *client = calloc(1, sizeof(*client));
	client->socket = -1;

	/* IMPLEMENT THIS FUNCTION */
	client->recieved = client->sent = client->to_send = 0;

	return client;
}

void chat_client_delete(struct chat_client *client)
{
	if (client->socket >= 0)
		close(client->socket);

	/* IMPLEMENT THIS FUNCTION */
	for (int i = 0; i < client->recieved; i++)
	{
		free(client->recieved_msgs[i].data);
	}
	for (int i = 0; i < client->sent; i++)
	{
		free(client->sent_msgs[i].data);
	}
	free(client);
}

int chat_client_connect(struct chat_client *client, const char *addr)
{
	/*
	 * 1) Use getaddrinfo() to resolve addr to struct sockaddr_in.
	 * 2) Create a client socket (function socket()).
	 * 3) Connect it by the found address (function connect()).
	 */
	if (client->socket != -1)
	{
		return CHAT_ERR_ALREADY_STARTED;
	}
	char *editable_addr = strdup(addr);
	char *host = strtok(editable_addr, ":");
	char *port = strtok(NULL, ":");
	if (host == NULL || port == NULL)
	{
		return CHAT_ERR_NO_ADDR;
	}

	struct addrinfo *address;
	struct addrinfo filter;
	memset(&filter, 0, sizeof(filter));
	filter.ai_family = AF_INET;
	filter.ai_socktype = SOCK_STREAM;
	int rc = getaddrinfo(host, port, &filter, &address);
	if (rc != 0)
	{
		return CHAT_ERR_NO_ADDR;
	}
	if (address == NULL)
	{
		freeaddrinfo(address);
		return CHAT_ERR_SYS;
	}
	for (struct addrinfo *prop = address; prop != NULL; prop = prop->ai_next)
	{
		client->socket = socket(prop->ai_family, prop->ai_socktype, prop->ai_protocol);
		if (client->socket == -1)
		{
			continue;
		}

		if (connect(client->socket, prop->ai_addr, prop->ai_addrlen) != -1)
		{
			break;
		}

		close(client->socket);
		client->socket = -1;
	}
	freeaddrinfo(address);

	if (client->socket == -1)
	{
		perror("Failed to connect");
		return CHAT_ERR_SYS;
	}

	client->fd.fd = client->socket;
	client->fd.events = POLLIN;
	return 0;
}

struct chat_message *
chat_client_pop_next(struct chat_client *client)
{
	/* IMPLEMENT THIS FUNCTION */
	(void)client;
	return NULL;
}

int chat_client_update(struct chat_client *client, double timeout)
{
	/*
	 * The easiest way to wait for updates on a single socket with a timeout
	 * is to use poll(). Epoll is good for many sockets, poll is good for a
	 * few.
	 *
	 * You create one struct pollfd, fill it, call poll() on it, handle the
	 * events (do read/write).
	 */

	if (client->socket == -1)
	{
		return CHAT_ERR_NOT_STARTED;
	}

	if (client->sent - client->to_send > 0)
	{
		client->fd.events |= POLLOUT;
	}
	client->fd.revents = 0;
	int ret = poll(&client->fd, 1, timeout * 1000);
	if (ret == -1)
	{
		perror("poll");
		
		return CHAT_ERR_SYS;
	}
	else if (ret == 0)
	{
		return CHAT_ERR_TIMEOUT;
	}
	
	if (client->fd.revents & POLLIN)
	{
		// printf("Incoming\n");
		// TODO: Handle incoming data
		char buffer[2048];
		ssize_t bytes_read = recv(client->socket, buffer, sizeof(buffer), 0);

		if (bytes_read == -1)
		{
			// Error handling

			perror("recv");
			return CHAT_ERR_SYS;
		}
		else if (bytes_read == 0)
		{
			// Connection closed
			printf("Server closed the connection\n");
			close(client->socket);
			return 0;
		}
		else
		{
			client->recieved_msgs[client->recieved].data = strdup(buffer);
			client->recieved_msgs[client->recieved].size = (int)bytes_read;
			client->recieved++;
		}
	}

	if (client->fd.revents & POLLOUT)
	{
		// printf("Outgoing\n");

		// TODO: Handle outgoing data
		
		ssize_t bytes_sent = send(client->socket, client->sent_msgs[client->to_send].data, client->sent_msgs[client->to_send].size, 0);
		if (bytes_sent == -1)
		{
			perror("send");
			
			return CHAT_ERR_SYS;
		}
		if(bytes_sent == 0)
		{
			close(client->socket);
			return 0;
		}
		client->to_send++;
		client->fd.events = POLLIN;
		// for (; client->to_send < client->sent; client->to_send++)
		// {
		// }
		// printf("%d %d\n", client->to_send, client->sent);
	}
	return 0;
}

int chat_client_get_descriptor(const struct chat_client *client)
{
	return client->socket;
}

int chat_client_get_events(const struct chat_client *client)
{
	/*
	 * IMPLEMENT THIS FUNCTION - add OUTPUT event if has non-empty output
	 * buffer.
	 */

	if (client->sent != 0)
	{
		return CHAT_EVENT_OUTPUT | CHAT_EVENT_INPUT;
	}
	if (client->socket == -1)
		return 0;
	return CHAT_EVENT_INPUT;
}

int chat_client_feed(struct chat_client *client, const char *msg, uint32_t msg_size)
{
	if (client->socket == -1)
	{
		return CHAT_ERR_NOT_STARTED;
	}
	bool new_msg = true;
	int cursor = 0;
	for (uint32_t i = 0; i < msg_size; i++)
	{
		if (new_msg)
		{

			client->sent_msgs[client->sent].data = malloc(sizeof(char) * 1024);
			// printf("HERE\n");
			new_msg = false;
			cursor = 0;
			client->sent_msgs[client->sent].size = 0;
		}
		client->sent_msgs[client->sent].data[cursor] = msg[i];
		client->sent_msgs[client->sent].size++;
		if (msg[i] == '\n')
		{
			client->fd.events |= POLLOUT;
			client->sent++;
			new_msg = true;
			continue;
		}
	}

	return 0;
}
