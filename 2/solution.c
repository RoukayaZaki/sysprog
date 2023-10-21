#include "parser.h"
#include <stdlib.h>
#include <assert.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <string.h>
#include <sys/wait.h>
#define _GNU_SOURCE
#include <fcntl.h>

void flush_to_stdout(int input_fd, enum output_type out_type, char *out_file)
{
	char buffer[BUFSIZ];
	int bytes_count = read(input_fd, buffer, BUFSIZ);
	int fd = STDOUT_FILENO;
	if (out_type == OUTPUT_TYPE_STDOUT)
	{
		// already default
	}
	else if (out_type == OUTPUT_TYPE_FILE_NEW)
	{
		fd = open(out_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
	}
	else if (out_type == OUTPUT_TYPE_FILE_APPEND)
	{
		fd = open(out_file, O_WRONLY | O_CREAT | O_APPEND, 0644);
	}
	else
	{
		assert(false);
	}
	if (fd == -1)
	{
		perror("open failed");
		exit(1);
	}
	write(fd, buffer, bytes_count);
	if (fd != STDOUT_FILENO)
		close(fd);
}

void debug_line(const struct command_line *line)
{
	fprintf(stderr, "================================\n");
	fprintf(stderr, "Command line:\n");
	fprintf(stderr, "Is background: %d\n", (int)line->is_background);
	fprintf(stderr, "Output: ");
	if (line->out_type == OUTPUT_TYPE_STDOUT)
	{
		fprintf(stderr, "stdout\n");
	}
	else if (line->out_type == OUTPUT_TYPE_FILE_NEW)
	{
		fprintf(stderr, "new file - \"%s\"\n", line->out_file);
	}
	else if (line->out_type == OUTPUT_TYPE_FILE_APPEND)
	{
		fprintf(stderr, "append file - \"%s\"\n", line->out_file);
	}
	else
	{
		assert(false);
	}
	fprintf(stderr, "Expressions:\n");
	const struct expr *e = line->head;
	while (e != NULL)
	{
		if (e->type == EXPR_TYPE_COMMAND)
		{
			fprintf(stderr, "\tCommand: %s", e->cmd.exe);
			for (uint32_t i = 0; i < e->cmd.arg_count; ++i)
				fprintf(stderr, " %s", e->cmd.args[i]);
			fprintf(stderr, "\n");
		}
		else if (e->type == EXPR_TYPE_PIPE)
		{
			fprintf(stderr, "\tPIPE\n");
		}
		else if (e->type == EXPR_TYPE_AND)
		{
			fprintf(stderr, "\tAND\n");
		}
		else if (e->type == EXPR_TYPE_OR)
		{
			fprintf(stderr, "\tOR\n");
		}
		else
		{
			assert(false);
		}
		e = e->next;
	}
}

static void
execute_command_line(const struct command_line *line)
{
	// debug_line(line);
	if (line->is_background)
	{
		int pid = fork();
		if (pid != 0)
			return;
	}
	// int last_command_status = 0;
	// while(last_command_status != -1) wait(&last_command_status);
	int last_stdout = STDIN_FILENO;
	const struct expr *e = line->head;
	int last_status = 0;
	int commands_ran = 0;
	if(strcmp(e->cmd.exe, "exit") == 0 && e->next == NULL) exit(0);
	while (e != NULL)
	{
		if (e->type == EXPR_TYPE_COMMAND)
		{
			if (last_status != 0)
			{
				goto next_command;
			}
			// fprintf(stderr, "\tCommand: %s", e->cmd.exe);
			// for (uint32_t i = 0; i < e->cmd.arg_count; ++i)
			// 	fprintf(stderr, " %s", e->cmd.args[i]);
			// fprintf(stderr, "\n");
			if (strcmp(e->cmd.exe, "cd") == 0)
			{
				if (chdir(e->cmd.args[0]) == 0)
				{
					// fprintf(stderr, "Current directory changed to: %s\n", e->cmd.args[0]);
					last_status = 0;
				}
				else
				{
					last_status = 1;
					perror("chdir");
				}
			}
			// else if (strcmp(e->cmd.exe, "exit") == 0 && e->cmd.arg_count <= 1)
			// {
			// 	if (e->next == NULL)
			// 		goto next_command;
			// 	// {
			// 	// 	// fprintf(stderr, "Exiting, bye bye\n");
			// 	// 	// wait(&last_status);
			// 	// 	// if(e->cmd.arg_count == 1){
			// 	// 	// 	int exit_code = atoi(e->cmd.args[0]);
			// 	// 	// 	exit(exit_code);
			// 	// 	// }
			// 	// 	// else 
			// 	// 	exit(0);
			// 	// }
			// }
			else
			{
				int fd[2];
				pipe(fd);

				// malloc args before forking so it's done just once and can be freed in the parent process
				char **arguments = malloc(sizeof(char *) * (e->cmd.arg_count + 1 + 1));
				arguments[0] = e->cmd.exe;
				for (uint32_t i = 0; i < e->cmd.arg_count; ++i)
					arguments[i + 1] = e->cmd.args[i];
				arguments[e->cmd.arg_count + 1] = NULL;
				commands_ran++;
				int pid = fork();
				if (pid == 0)
				{
					close(fd[0]);
					dup2(last_stdout, STDIN_FILENO);
					dup2(fd[1], STDOUT_FILENO);
					execvp(e->cmd.exe, arguments);
					// perror("execvp");
					commands_ran--;
					exit(1);
				}
				close(fd[1]);
				// wait(&last_status);
				last_stdout = fd[0];
				// fprintf(stderr, "Changing stdout to: %d\n", last_stdout);
				free(arguments);
			}
		}
		else if (e->type == EXPR_TYPE_PIPE)
		{
			// Already handled
		}
		else if (e->type == EXPR_TYPE_AND)
		{
			while (commands_ran-- > 0)
			{
				wait(&last_status);
			}
			commands_ran = 0;
			if (last_status == 0)
			{
				flush_to_stdout(last_stdout, line->out_type, line->out_file);
			}
			else
			{
				goto next_command;
			}
			last_stdout = STDIN_FILENO; // reset the piping behavior
		}
		else if (e->type == EXPR_TYPE_OR)
		{
			while (commands_ran-- > 0)
			{
				wait(&last_status);
			}
			commands_ran = 0;
			if (last_status == 0)
			{
				// Skip upcoming commands
				break;
			}
			last_stdout = STDIN_FILENO; // reset the piping behavior
			last_status = 0;
		}
		else
		{
			assert(false);
		}
	next_command:
		e = e->next;
	}

	while (commands_ran-- > 0)
	{
		//  wait(&last_status);
	}
	commands_ran = 0;

	if (last_status != 0)
	{
		// fprintf(stderr, "An error occurred!\n");
		// return;
	}

	if (last_stdout != STDIN_FILENO)
		flush_to_stdout(last_stdout, line->out_type, line->out_file);
}

int main(void)
{
	const size_t buf_size = 1024;
	char buf[buf_size];
	int rc;
	struct parser *p = parser_new();
	// printf("$> ");
	fflush(stdout);
	while ((rc = read(STDIN_FILENO, buf, buf_size)) > 0)
	{
		parser_feed(p, buf, rc);
		struct command_line *line = NULL;
		while (true)
		{
			enum parser_error err = parser_pop_next(p, &line);
			if (err == PARSER_ERR_NONE && line == NULL)
				break;
			// debug_line(line);
			if (err != PARSER_ERR_NONE)
			{
				printf("Error: %d\n", (int)err);
				continue;
			}
			execute_command_line(line);
			command_line_delete(line);
		}
		// printf("$> ");
		fflush(stdout);
	}
	parser_delete(p);
	return 0;
}
