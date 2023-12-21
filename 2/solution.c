#include "parser.h"
#include <stdlib.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <string.h>
#include <sys/wait.h>
#include <stdbool.h>
#define _GNU_SOURCE
#include <fcntl.h>
// return last command status
static int
execute_command_line(struct command_line *line, struct parser *p)
{
	assert(line != NULL);

	int last_status = 0;
	if (line->is_background)
	{
		int pid = fork();
		if (pid != 0)
			return last_status;
	}
	int stdout = dup(STDOUT_FILENO);
	if (line->out_type == OUTPUT_TYPE_STDOUT)
	{
	}
	else if (line->out_type == OUTPUT_TYPE_FILE_NEW)
	{
		int out_file = open(line->out_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
		dup2(out_file, STDOUT_FILENO);
	}
	else if (line->out_type == OUTPUT_TYPE_FILE_APPEND)
	{
		int out_file = open(line->out_file, O_WRONLY | O_CREAT | O_APPEND, 0644);
		dup2(out_file, STDOUT_FILENO);
	}
	else
	{
		assert(false);
	}
	int pipes_num = 0, pipe_idx = 0;
	for (const struct expr *x = line->head; x != NULL; x = x->next)
	{
		if (x->type == EXPR_TYPE_PIPE)
			pipes_num++;
	}
	int pipes[pipes_num][2];
	for (int i = 0; i < pipes_num; i++)
	{
		pipe(pipes[i]);
	}
	// int last_exit_code = 0;
	bool read_from_pipe = false;
	int forks = 0;
	const struct expr *e = line->head;
	if (strcmp(e->cmd.exe, "exit") == 0 && e->next == NULL)
	{
		if (e->cmd.arg_count > 0)
		{
			last_status = atoi(e->cmd.args[0]);
		}
		command_line_delete(line);
		parser_delete(p);

		exit(last_status);
	}
	while (e != NULL)
	{
		if (e->type == EXPR_TYPE_COMMAND)
		{
			if (strcmp(e->cmd.exe, "cd") == 0)
			{
				if (chdir(e->cmd.args[0]) != 0)
				{
					perror("chdir");
				}
			}
			else
			{
				// malloc args before forking so it's done just once and can be freed in the parent process

				int pid = fork();
				forks++;
				if (pid == 0)
				{
					char **arguments = malloc(sizeof(char *) * (e->cmd.arg_count + 1 + 1));
					arguments[0] = strdup(e->cmd.exe);
					for (uint32_t i = 0; i < e->cmd.arg_count; ++i)
					{
						arguments[i + 1] = e->cmd.args[i];
					}
					arguments[e->cmd.arg_count + 1] = '\0';
					if (read_from_pipe)
					{
						// close(pipes[pipe_idx - 1][1]);
						dup2(pipes[pipe_idx - 1][0], STDIN_FILENO);
						// read_from_pipe = false;
					}
					if (e->next != NULL && e->next->type == EXPR_TYPE_PIPE)
					{
						close(pipes[pipe_idx][0]);
						dup2(pipes[pipe_idx][1], STDOUT_FILENO);
					}
					int status = execvp(e->cmd.exe, arguments);
					command_line_delete(line);
					parser_delete(p);
					free(arguments[0]);
					free(arguments);
					exit(status);
				}
				if (e->next == NULL || e->next->type != EXPR_TYPE_PIPE)
				{
					int wait_var;
					waitpid(pid, &wait_var, 0);
					last_status = WEXITSTATUS(wait_var);
					forks--;
				}
				if (strcmp(e->cmd.exe, "exit") == 0)
				{
					if (e->cmd.arg_count > 0)
					{
						last_status = atoi(e->cmd.args[0]);
						// printf("saw exit: %d\n", *last_status);
					}
					else
						last_status = 0;
				}

				if (read_from_pipe)
				{
					close(pipes[pipe_idx - 1][0]);
					read_from_pipe = false;
				}
			}
		}
		else if (e->type == EXPR_TYPE_PIPE)
		{
			close(pipes[pipe_idx][1]);
			pipe_idx++;
			read_from_pipe = true;
		}
		else if (e->type == EXPR_TYPE_AND)
		{
			// printf("\tAND\n");
			read_from_pipe = false;
		}
		else if (e->type == EXPR_TYPE_OR)
		{
			// printf("\tOR\n");
			read_from_pipe = false;
		}
		else
		{
			assert(false);
		}
		e = e->next;
	}
	// for(int i = 0; i < pipes_num; i++) close(pipes[i][0]);
	int wait_status;
	while (forks--)
		wait(&wait_status);
	dup2(stdout, STDOUT_FILENO);
	return last_status;
}

int main(void)
{
	const size_t buf_size = 1024;
	char buf[buf_size];
	int rc;
	int last_status = 0;
	struct parser *p = parser_new();
	while ((rc = read(STDIN_FILENO, buf, buf_size)) > 0)
	{
		parser_feed(p, buf, rc);
		struct command_line *line = NULL;
		while (true)
		{
			enum parser_error err = parser_pop_next(p, &line);
			if (err == PARSER_ERR_NONE && line == NULL)
				break;
			if (err != PARSER_ERR_NONE)
			{
				printf("Error: %d\n", (int)err);
				continue;
			}
			last_status = execute_command_line(line, p);
			command_line_delete(line);
		}
	}
	parser_delete(p);
	return last_status;
}