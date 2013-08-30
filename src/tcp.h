#ifndef TCP_H
#define TCP_H

ssize_t my_buffered_read(int fd, char *ptr, size_t maxlen);
ssize_t my_readline(int fd, void *vptr, size_t maxlen);
int my_writen(int fd, const void *ptr, size_t nbytes);

#endif
