#include "so_stdio.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#define BUFSIZE 4096

typedef struct _so_file {
	int fd;
	int err;
	unsigned char *readBuffer;
	unsigned char *writeBuffer;
	int readIndex;
	int writeIndex;
	ssize_t bytes_read;
	ssize_t bytes_write;
	ssize_t cursor;
	pid_t childPid;
	pid_t parentPid;
	int isPipe;
} SO_FILE;

SO_FILE *so_fopen(const char *pathname, const char *mode)
{
	SO_FILE *ret = (SO_FILE *) malloc(sizeof(SO_FILE));

	if (ret == NULL)
		return NULL;

	ret->err = 0;
	ret->readIndex = 0;
	ret->writeIndex = 0;
	ret->bytes_read = 0;
	ret->bytes_write = 0;
	ret->cursor = 0;
	ret->childPid = 0;
	ret->parentPid = 0;
	ret->isPipe = 0;
	ret->fd = -1;

	if (strcmp(mode, "r") == 0)
		ret->fd = open(pathname, O_RDONLY);
	else if (strcmp(mode, "r+") == 0)
		ret->fd = open(pathname, O_RDWR);
	else if (strcmp(mode, "w") == 0)
		ret->fd = open(pathname, O_WRONLY | O_CREAT | O_TRUNC, 0644);
	else if (strcmp(mode, "w+") == 0)
		ret->fd = open(pathname, O_RDWR | O_CREAT | O_TRUNC, 0644);
	else if (strcmp(mode, "a") == 0)
		ret->fd = open(pathname, O_WRONLY | O_CREAT | O_APPEND, 0644);
	else if (strcmp(mode, "a+") == 0)
		ret->fd = open(pathname, O_RDWR | O_CREAT | O_APPEND, 0644);

	if (ret->fd == -1) {
		free(ret);
		return NULL;
	}

	ret->readBuffer = (unsigned char *) malloc((BUFSIZE + 1) * sizeof(unsigned char));
	if (ret->readBuffer == NULL) {
		free(ret);
		return NULL;
	}

	ret->writeBuffer = (unsigned char *) malloc((BUFSIZE + 1) * sizeof(unsigned char));
	if (ret->writeBuffer == NULL) {
		free(ret->readBuffer);
		free(ret);
		return NULL;
	}

	return ret;
}

int so_fclose(SO_FILE *stream)
{
	int rc;
	int ret = 0;

	if (stream->writeIndex != 0) {
		rc = so_fflush(stream);
		if (rc == SO_EOF)
			ret = -1;
	}

	free(stream->readBuffer);
	free(stream->writeBuffer);

	rc = close(stream->fd);
	free(stream);

	if (rc < 0)
		ret = -1;

	return ret;

}

int so_fflush(SO_FILE *stream)
{
	if (stream->writeIndex == 0)
		return SO_EOF;

	ssize_t bytes_write_now = 0;

	while (bytes_write_now < stream->writeIndex) {
		stream->bytes_write = write(stream->fd, stream->writeBuffer + bytes_write_now, stream->writeIndex - bytes_write_now);
		if (stream->bytes_write < 0)
			return SO_EOF;

		bytes_write_now += stream->bytes_write;
	}

	memset(stream->writeBuffer, '\0', (BUFSIZE + 1) * sizeof(unsigned char));
	stream->writeIndex = 0;

	return 0;
}

int so_fileno(SO_FILE *stream)
{
	return stream->fd;
}

int so_fseek(SO_FILE *stream, long offset, int whence)
{
	int rc;

	stream->readIndex = 0;
	memset(stream->readBuffer, '\0', (BUFSIZE + 1) * sizeof(unsigned char));

	so_fflush(stream);

	rc = lseek(stream->fd, offset, whence);
	if (rc < 0)
		return -1;

	stream->cursor = rc;

	return 0;
}

long so_ftell(SO_FILE *stream)
{
	return stream->cursor;
}

size_t so_fread(void *ptr, size_t size, size_t nmemb, SO_FILE *stream)
{
	int size_read = 0;

	for (int i = 0; i < nmemb; i++) {
		for (int j = 0; j < size; j++) {
			if (ptr == NULL)
				return 0;

			int rc = so_fgetc(stream);

			if (rc == SO_EOF) {
				if (stream->isPipe) {
					stream->isPipe = 0;
					stream->cursor = 0;
				}

				return size_read;
			}

			*(char *)ptr = (char)rc;
			ptr++;
		}
		size_read++;
	}

	stream->cursor -= nmemb * size;
	stream->cursor += size_read;
	return size_read;
}

size_t so_fwrite(const void *ptr, size_t size, size_t nmemb, SO_FILE *stream)
{
	int size_write = 0;

	for (int i = 0; i < nmemb; i++) {
		for (int j = 0; j < size; j++) {
			if (ptr == NULL)
				return 0;

			int rc = so_fputc(*(int *)ptr, stream);

			if (rc == SO_EOF)
				return 0;

			ptr++;
		}
		size_write++;
	}

	stream->cursor += size_write;
	return size_write;
}

int so_fgetc(SO_FILE *stream)
{
	if (stream->readIndex == 0 || stream->readIndex == stream->bytes_read) {
		stream->readIndex = 0;
		memset(stream->readBuffer, '\0', (BUFSIZE + 1) * sizeof(unsigned char));

		stream->bytes_read = read(stream->fd, stream->readBuffer, BUFSIZE);
		if (stream->bytes_read <= 0) {
			stream->cursor++;
			stream->err++;
			return SO_EOF;
		}

		stream->readBuffer[stream->bytes_read] = '\0';
	}
	stream->cursor++;
	return stream->readBuffer[stream->readIndex++];
}

int so_fputc(int c, SO_FILE *stream)
{
	if (stream->writeIndex == BUFSIZE) {
		int rc = so_fflush(stream);

		if (rc == SO_EOF) {
			stream->err++;
			return SO_EOF;
		}
	}

	stream->writeBuffer[stream->writeIndex++] = (unsigned char)c;

	return c;
}

int so_feof(SO_FILE *stream)
{
	if (stream->isPipe)
		return 0;

	int ret = 0;
	int rc = lseek(stream->fd, 0, SEEK_END);

	if (rc + 1 == stream->cursor)
		ret = -1;

	lseek(stream->fd, stream->cursor, SEEK_SET);

	return ret;
}

int so_ferror(SO_FILE *stream)
{
	if (stream->err > 0)
		return -1;

	return 0;
}

SO_FILE *so_popen(const char *command, const char *type)
{
	SO_FILE *f = (SO_FILE *) malloc(sizeof(SO_FILE));
	int rc;
	pid_t pid;
	int status;
	int result;
	int fds[2];

	rc = pipe(fds);
	pid = fork();

	switch (pid) {
	case -1:
		close(fds[0]);
		close(fds[1]);
		free(f);
		return NULL;

	case 0:
		if (*type == 'r')
			dup2(fds[1], STDOUT_FILENO);
		else
			dup2(fds[0], STDIN_FILENO);

		close(fds[0]);
		close(fds[1]);

		execl("/bin/sh", "sh", "-c", command, NULL);
		exit(EXIT_FAILURE);

	default:
		break;
	}

	if (*type == 'r')
		f->fd = dup(fds[0]);
	else
		f->fd = dup(fds[1]);

	close(fds[0]);
	close(fds[1]);

	f->readBuffer = (unsigned char *) malloc((BUFSIZE + 1) * sizeof(unsigned char));
	f->writeBuffer = (unsigned char *) malloc((BUFSIZE + 1) * sizeof(unsigned char));
	memset(f->readBuffer, '\0', (BUFSIZE + 1) * sizeof(unsigned char));
	memset(f->writeBuffer, '\0', (BUFSIZE + 1) * sizeof(unsigned char));
	f->readIndex = 0;
	f->writeIndex = 0;
	f->bytes_read = 0;
	f->bytes_write = 0;
	f->cursor = 0;
	f->childPid = pid;
	f->parentPid = getpid();
	f->isPipe = 1;
	f->err = 0;

	return f;
}

int so_pclose(SO_FILE *stream)
{
	pid_t childPid = stream->childPid;
	pid_t parentPid = stream->parentPid;
	int status = -1;

	so_fclose(stream);

	waitpid(childPid, &status, 0);
	if (!WIFEXITED(status))
		return -1;

	waitpid(parentPid, &status, 0);
	if (!WIFEXITED(status))
		return -1;

	return 0;
}
