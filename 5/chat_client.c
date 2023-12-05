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
#define _GNU_SOURCE

#define MAX_EVENTS 1024
struct chat_client
{
	/** Socket connected to the server. */
	int socket;
	/** Array of received messages. */
	struct chat_message recieved_msgs[1024];
	/* ... */
	/** Output buffer. */
	char *sent_buffer;
	char *last_message;
	/* ... */
	/* PUT HERE OTHER MEMBERS */
	int sending;
	int sent;
	int recieved;
	int size;
	int cursor;
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
	client->recieved = client->sending = client->sent = client->size = client->cursor = 0;
	client->sent_buffer = client->last_message = NULL;

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
	free(client->last_message);
	free(client->sent_buffer);
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
			close(client->socket);
			client->socket = -1;
			continue;
		}

		if (connect(client->socket, prop->ai_addr, prop->ai_addrlen) != -1)
		{
			break;
		}
	}
	freeaddrinfo(address);
	free(editable_addr);
	// free(host);
	// free(port);
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

	if (client->sending - client->sent > 0)
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
		ssize_t total_bytes_sent = 0;
		while ((int)total_bytes_sent < client->size)
		{
			ssize_t bytes_sent = send(client->socket, client->sent_buffer + total_bytes_sent, client->size - total_bytes_sent, 0);
			if (bytes_sent == -1)
			{
				perror("send");

				return CHAT_ERR_SYS;
			}
			if (bytes_sent == 0)
			{
				close(client->socket);
				return 0;
			}
			// printf("Bytes sending: %ld Message: %s\n", bytes_sent, client->sent_msgs[client->sent].data);
			total_bytes_sent += bytes_sent;
		}
		client->fd.events = POLLIN;
		client->size = 0;
		free(client->sent_buffer);
		client->sent_buffer = NULL;
		// for (; client->sent < client->sending; client->sent++)
		// {
		// }
		// printf("%d %d\n", client->sent, client->sending);
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

	if (client->sending != 0)
	{
		return CHAT_EVENT_OUTPUT | CHAT_EVENT_INPUT;
	}
	if (client->socket == -1)
		return 0;
	return CHAT_EVENT_INPUT;
}
// reference: https://stackoverflow.com/questions/122616/how-do-i-trim-leading-trailing-whitespace-in-a-standard-way
char *trimwhitespace(char *str)
{
	char *end;

	while (isspace((unsigned char)*str))
		str++;

	if (*str == 0)
		return str;

	end = str + strlen(str) - 1;
	while (end > str && isspace((unsigned char)*end))
		end--;

	end[1] = '\0';

	return str;
}
int chat_client_feed(struct chat_client *client, const char *msg, uint32_t msg_size)
{
	if (client->socket == -1)
	{
		return CHAT_ERR_NOT_STARTED;
	}
	char *modifiable_msg = strdup(msg);

	int size = 0, start = 0;
	if (client->last_message == NULL)
		client->last_message = calloc(msg_size, sizeof(char));
	else
	{
		size = client->cursor;
		client->last_message = realloc(client->last_message, (int)size + msg_size);
	}
	for (int i = 0; i <= (int)msg_size; i++)
	{
		client->last_message[size] = modifiable_msg[i];
		size++;
		if (modifiable_msg[i] == '\n')
		{
			client->last_message[size] = '\0';
			client->last_message = trimwhitespace(client->last_message);
			client->sent_buffer = realloc(client->sent_buffer, client->size + size);
			memcpy(client->sent_buffer + client->size, client->last_message, size);
			client->size += size;
			size = 0;
			client->cursor = 0;
			start = i + 1;
			client->fd.events |= POLLOUT;
		}
	}
	free(modifiable_msg);
	return 0;
}
