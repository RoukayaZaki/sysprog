#include "userfs.h"
#include <stddef.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
enum
{
	BLOCK_SIZE = 512,
	MAX_FILE_SIZE = 1024 * 1024 * 100,
};

/** Global error code. Set from any function on any error. */
static enum ufs_error_code ufs_error_code = UFS_ERR_NO_ERR;

struct block
{
	/** Block memory. */
	char *memory;
	/** How many bytes are occupied. */
	int occupied;
	/** Next block in the file. */
	struct block *next;
	/** Previous block in the file. */
	struct block *prev;

	/* PUT HERE OTHER MEMBERS */
};

struct file
{
	/** Double-linked list of file blocks. */
	struct block *block_list;
	/**
	 * Last block in the list above for fast access to the end
	 * of file.
	 */
	struct block *last_block;
	/** How many file descriptors are opened on the file. */
	int refs;
	/** File name. */
	char *name;
	/** Files are stored in a double-linked list. */
	struct file *next;
	struct file *prev;

	/* PUT HERE OTHER MEMBERS */
	int block_num;
	bool is_deleted;
};

/** List of all files. */
static struct file *file_list = NULL;

struct filedesc
{
	struct file *file;

	/* PUT HERE OTHER MEMBERS */
	int cursor;
	int flags;
};

/**
 * An array of file descriptors. When a file descriptor is
 * created, its pointer drops here. When a file descriptor is
 * closed, its place in this array is set to NULL and can be
 * taken by next ufs_open() call.
 */
static struct filedesc **file_descriptors = NULL;
static int file_descriptor_count = 0;
static int file_descriptor_capacity = 0;

enum ufs_error_code
ufs_errno()
{
	return ufs_error_code;
}

int ufs_open(const char *filename, int flags)
{

	struct file *new_file;
	bool exists = false;
	for (struct file *f = file_list; f != NULL; f = f->next)
	{
		if (strcmp(f->name, filename) == 0)
		{
			exists = true;
			new_file = f;
			break;
		}
	}
	if (!exists || (exists && new_file->is_deleted))
	{
		if (!(flags & UFS_CREATE))
		{
			ufs_error_code = UFS_ERR_NO_FILE;
			return -1;
		}
		new_file = malloc(sizeof(struct file));
		new_file->block_list = NULL;
		new_file->last_block = NULL;
		new_file->block_num = 0;
		new_file->name = strdup(filename);
		new_file->refs = 0;
		new_file->next = file_list;
		new_file->prev = NULL;
		new_file->is_deleted = false;
		if (file_list != NULL)
		{
			file_list->prev = new_file;
		}
		file_list = new_file;
	}

	new_file->refs++;
	struct filedesc *desc = malloc(sizeof(struct filedesc));
	desc->file = new_file;
	desc->cursor = 0;
	if (flags & UFS_READ_ONLY)
	{
		desc->flags = 1;
	}
	else if (flags & UFS_WRITE_ONLY)
	{
		desc->flags = 2;
	}
	else
	{
		desc->flags = 3;
	}
	if (file_descriptor_count == file_descriptor_capacity)
	{
		file_descriptor_capacity++;
		file_descriptors = realloc(file_descriptors, sizeof(struct filedesc *) * file_descriptor_capacity);
		file_descriptors[file_descriptor_count] = desc;
		file_descriptor_count++;
		return file_descriptor_count - 1;
	}
	for (int i = 0; i < file_descriptor_capacity; i++)
	{
		if (file_descriptors[i] == NULL)
		{
			file_descriptors[i] = desc;
			file_descriptor_count++;
			return i;
		}
	}
	return 0;
}
static struct block *create_block()
{
	struct block *new_block = malloc(sizeof(struct block));
	if (new_block == NULL)
	{
		return NULL;
	}
	new_block->memory = malloc(BLOCK_SIZE);
	if (new_block->memory == NULL)
	{
		free(new_block);
		return NULL;
	}
	new_block->occupied = 0;
	new_block->next = NULL;
	new_block->prev = NULL;

	return new_block;
}

ssize_t
ufs_write(int fd, const char *buf, size_t size)
{
	if (fd >= file_descriptor_capacity || fd < 0)
	{
		ufs_error_code = UFS_ERR_NO_FILE;
		return -1;
	}
	struct filedesc *desc = file_descriptors[fd];
	if (desc == NULL || desc->file == NULL)
	{
		ufs_error_code = UFS_ERR_NO_FILE;
		return -1;
	}
	if (desc->flags == 1)
	{
		ufs_error_code = UFS_ERR_NO_PERMISSION;
		return -1;
	}

	struct file *f = desc->file;
	if (f->block_list == NULL)
	{
		struct block *b = create_block();
		if (b == NULL)
		{
			ufs_error_code = UFS_ERR_NO_MEM;
			return -1;
		}
		f->block_list = f->last_block = b;
		f->block_num++;
	}
	struct block *current_block = f->block_list;
	int cursor = desc->cursor % BLOCK_SIZE, block_idx = desc->cursor / BLOCK_SIZE;
	while (block_idx--)
	{
		current_block = current_block->next;
	}

	if (current_block == NULL)
	{
		if (f->block_num >= MAX_FILE_SIZE / BLOCK_SIZE)
		{
			ufs_error_code = UFS_ERR_NO_MEM;
			return -1;
		}
		f->block_num++;
		struct block *b = create_block();
		if (b == NULL)
		{
			ufs_error_code = UFS_ERR_NO_MEM;
			return -1;
		}
		b->prev = f->last_block;
		f->last_block->next = current_block = b;
		f->last_block = b;
		cursor = 0;
	}
	size_t bytes_written = 0;
	while (bytes_written < size)
	{
		size_t bytes_to_write = ((size - bytes_written) < (size_t)(BLOCK_SIZE - cursor)) ? (size - bytes_written) : (size_t)(BLOCK_SIZE - cursor);
		memcpy(current_block->memory + cursor, buf + bytes_written, bytes_to_write);
		current_block->occupied = ((int)(cursor + bytes_to_write) > current_block->occupied) ? (int)(cursor + bytes_to_write) : current_block->occupied;
		bytes_written += bytes_to_write;
		if (current_block->next == NULL && bytes_written < size)
		{

			struct block *b = create_block();
			f->block_num++;
			if (b == NULL || f->block_num > MAX_FILE_SIZE / BLOCK_SIZE)
			{
				ufs_error_code = UFS_ERR_NO_MEM;
				return -1;
			}
			current_block->next = b;
			b->prev = current_block;
			f->last_block = b;
		}
		cursor = 0;
		current_block = current_block->next;
	}
	desc->cursor += bytes_written;
	return bytes_written;
}

ssize_t
ufs_read(int fd, char *buf, size_t size)
{
	if (fd < 0 || fd >= file_descriptor_capacity)
	{
		ufs_error_code = UFS_ERR_NO_FILE;
		return -1;
	}

	struct filedesc *desc = file_descriptors[fd];
	if (desc == NULL || desc->file == NULL)
	{
		ufs_error_code = UFS_ERR_NO_FILE;
		return -1;
	}
	if (desc->flags == 2)
	{
		ufs_error_code = UFS_ERR_NO_PERMISSION;
		return -1;
	}
	struct file *f = desc->file;
	struct block *current_block = f->block_list;
	int cursor = desc->cursor % BLOCK_SIZE, block_idx = desc->cursor / BLOCK_SIZE;
	while (block_idx--)
	{
		current_block = current_block->next;
	}
	size_t bytes_read = 0;

	while (bytes_read < size && current_block != NULL)
	{
		size_t bytes_available = current_block->occupied - cursor;

		if (bytes_available > 0)
		{
			size_t bytes_to_read = (size - bytes_read < bytes_available) ? (size - bytes_read) : bytes_available;

			memcpy(buf + bytes_read, current_block->memory + cursor, bytes_to_read);
			cursor += bytes_to_read;
			bytes_read += bytes_to_read;
		}

		if (cursor >= current_block->occupied)
		{
			current_block = current_block->next;
			cursor = 0;
		}
	}
	
	desc->cursor += bytes_read;
	return bytes_read;
}
void delete_file(struct file *file)
{
	if (file == NULL)
	{
		return;
	}

	if (file == file_list)
	{
		file_list = file->next;
	}
	if (file->next != NULL)
	{
		file->next->prev = file->prev;
	}
	if (file->prev != NULL)
	{
		file->prev->next = file->next;
	}

	free(file->name);

	struct block *current_block = file->last_block;
	while (current_block != NULL)
	{
		struct block *next_block = current_block->prev;
		free(current_block->memory);
		free(current_block);
		current_block = next_block;
	}

	free(file);
}

int ufs_close(int fd)
{
	if (fd < 0 || fd >= file_descriptor_capacity)
	{
		ufs_error_code = UFS_ERR_NO_FILE;
		return -1;
	}
	if (file_descriptors[fd] == NULL || file_descriptors[fd]->file == NULL)
	{
		ufs_error_code = UFS_ERR_NO_FILE;
		return -1;
	}
	file_descriptor_count--;
	file_descriptors[fd]->file->refs--;
	if (file_descriptors[fd]->file->refs == 0 && file_descriptors[fd]->file->is_deleted)
	{
		delete_file(file_descriptors[fd]->file);
	}
	free(file_descriptors[fd]);
	file_descriptors[fd] = NULL;
	return 0;
}

int ufs_delete(const char *filename)
{
	struct file *file_to_be_deleted;
	bool exists = false;
	for (struct file *f = file_list; f != NULL; f = f->next)
	{
		if (strcmp(f->name, filename) == 0)
		{
			exists = true;
			file_to_be_deleted = f;
			break;
		}
	}
	if (!exists || (exists && file_to_be_deleted->is_deleted))
	{
		ufs_error_code = UFS_ERR_NO_FILE;
		return -1;
	}
	file_to_be_deleted->is_deleted = true;
	if (file_to_be_deleted->refs)
		return 0;

	delete_file(file_to_be_deleted);
	return 0;
}


void ufs_destroy(void)
{
	for (int fd = 0; fd < file_descriptor_capacity; fd++)
	{
		if (file_descriptors[fd] != NULL)
		{
			if (file_descriptors[fd]->file != NULL)
				file_descriptors[fd]->file->is_deleted = true;
			ufs_close(fd);
			free(file_descriptors[fd]);
		}
	}

	free(file_descriptors);

	while (file_list != NULL)
	{
		delete_file(file_list);
	}

	file_descriptor_count = 0;
	file_descriptor_capacity = 0;
	file_list = NULL;
	ufs_error_code = UFS_ERR_NO_ERR;
}