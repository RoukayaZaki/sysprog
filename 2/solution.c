#include "parser.h"
#include <stdlib.h>
#include <assert.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <string.h>
#include <sys/wait.h>
#include <stdbool.h>
#define _GNU_SOURCE
#include <fcntl.h>
// return last command status
static void
execute_command_line(const struct command_line *line)
{
	assert(line != NULL);

	if (line->is_background)
	{
		int pid = fork();
		if (pid != 0)
			return;
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
	for(int i = 0; i < pipes_num; i++)
	{
		pipe(pipes[i]);
	}
	// int last_exit_code = 0;
	bool read_from_pipe = false;
	int last_status, forks = 0;
	const struct expr *e = line->head;
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
				if (strcmp(e->cmd.exe, "exit"))
				{
				}
				// malloc args before forking so it's done just once and can be freed in the parent process
				char **arguments = malloc(sizeof(char *) * (e->cmd.arg_count + 1 + 1));
				arguments[0] = strdup(e->cmd.exe);
				for (uint32_t i = 0; i < e->cmd.arg_count; ++i)
				{
					arguments[i + 1] = e->cmd.args[i];
				}
				arguments[e->cmd.arg_count + 1] = NULL;

				int pid = fork();
					forks++;
				if (pid == 0)
				{
					if(read_from_pipe)
					{
						// close(pipes[pipe_idx - 1][1]);
						dup2(pipes[pipe_idx -1][0], STDIN_FILENO);
						// read_from_pipe = false;
					}
					if(e->next != NULL && e->next->type == EXPR_TYPE_PIPE)
					{
						close(pipes[pipe_idx][0]);
						dup2(pipes[pipe_idx][1], STDOUT_FILENO);
					}
					// printf("HERE\n");
					
					execvp(e->cmd.exe, arguments);
					// perror("execvp");
					exit(0);
				}
				// if(pid != 0)
				// 	waitpid(pid, &last_status, 0);
				if(read_from_pipe)
				{
					close(pipes[pipe_idx - 1][0]);
					read_from_pipe = false;
				}
				free(arguments);
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
	while(forks--)
		wait(&last_status);
	dup2(stdout, STDOUT_FILENO);
}

int main(void)
{
	const size_t buf_size = 1024;
	char buf[buf_size];
	int rc;
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
			execute_command_line(line);
			command_line_delete(line);
		}
	}
	parser_delete(p);
	return 0;
}