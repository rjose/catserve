#include <err.h>
#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>

#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>

/*==============================================================================
 * Defines
 */

#define MAXLINE 1024


/*==============================================================================
 * Static declarations
 */

/* Thread-specific data for thread-safe reading */
typedef struct {
	int rl_cnt;
	char *rl_bufptr;
	char rl_buf[MAXLINE];
} Rline;

static pthread_key_t rl_key;
static pthread_once_t rl_once = PTHREAD_ONCE_INIT;

static Rline *get_tsd();
static ssize_t my_read(Rline *, int, char *);
static void readline_destructor(void *);
static void readline_once();
static ssize_t writen(int, const void *, size_t);


/*==============================================================================
 * Public API
 */


/*------------------------------------------------------------------------------
 * Reads maxlen characters from fd into ptr using a thread-specific buffer.
 */
ssize_t
my_buffered_read(int fd, char *ptr, size_t maxlen)
{
        size_t n, rc;
        char c;
	Rline *tsd;

        /*
         * Read characters up to maxlen or until we get EOF
         */
        tsd = get_tsd();
	for (n=1; n <= maxlen; n++) {
		if ( (rc = my_read(tsd, fd, &c)) == 1) {
			*ptr++ = c;
		}
		else if (rc == 0) { 	/* Got EOF */
			*ptr = 0;
			return (n - 1);
		}
		else
			return -1;
	}

	*ptr = 0;
	return n - 1;
}


/*------------------------------------------------------------------------------
 * Reads a line of text up to maxlen characters.
 */
ssize_t
my_readline(int fd, void *vptr, size_t maxlen)
{
	size_t n, rc;
	char c, *ptr;
	Rline *tsd;

        /* Read up to '\n', maxlen chars, or EOF */
        tsd = get_tsd();
	ptr = vptr;
	for (n=1; n < maxlen; n++) {
		if ( (rc = my_read(tsd, fd, &c)) == 1) {
			*ptr++ = c;
			if (c == '\n')
				break;
		}
		else if (rc == 0) { 	/* Got EOF */
			*ptr = 0;
			return (n - 1);
		}
		else
			return -1;
	}

	*ptr = 0;
	return n;
}

/*------------------------------------------------------------------------------
 * Writes nbytes bytes to file descriptor
 */
int
my_writen(int fd, const void *ptr, size_t nbytes)
{
        int result = 0;
	if (writen(fd, ptr, nbytes) != nbytes) {
		warn("writen error");
                result = errno;
        }
        return result;
}



/*==============================================================================
 * Static functions
 */


/*------------------------------------------------------------------------------
 * Gets thread-specific data, ensuring it's initialized.
 */
static
Rline *get_tsd()
{
	Rline *tsd;

        /*
         * Initialize thread specific data if needed.
         */
	if ( pthread_once(&rl_once, readline_once) != 0)
		err(1, "Problem with pthread_once");

	if ( (tsd = pthread_getspecific(rl_key)) == NULL ) {
		tsd = calloc(1, sizeof(Rline));
		if (tsd == NULL)
			err(1, "Problem with calloc");
		if ( pthread_setspecific(rl_key, tsd) != 0)
			err(1, "Problem with pthread_setspecific");
	}

        return tsd;
}


/*------------------------------------------------------------------------------
 * Does threadsafe read from a file descriptor.
 */
static ssize_t
my_read(Rline *tsd, int fd, char *ptr)
{
	if (tsd->rl_cnt <= 0) {
again:
		if ( (tsd->rl_cnt = read(fd, tsd->rl_buf, MAXLINE)) < 0 ) {
			if (errno == EINTR)
				goto again;
			return -1;
		}
		else if (tsd->rl_cnt == 0)
			return 0;
		tsd->rl_bufptr = tsd->rl_buf; 	/* Reset rl_bufptr to beginning */
	}

	tsd->rl_cnt--;
	*ptr = *tsd->rl_bufptr++;
	return 1;
}


/*------------------------------------------------------------------------------
 * Frees thread-specific data when thread is done.
 */
static void
readline_destructor(void *ptr)
{
	free(ptr);
}


/*------------------------------------------------------------------------------
 * Creates some thread specific data.
 */
static void
readline_once()
{
	if (pthread_key_create(&rl_key, readline_destructor) != 0)
		err(1, "Problem creating pthread key");
}


/*------------------------------------------------------------------------------
 * Writes n bytes to descriptor
 */
static ssize_t
writen(int fd, const void *vptr, size_t n)
{
	size_t nleft;
	ssize_t nwritten;
	const char *ptr;

	ptr = vptr;
	nleft = n;
	while (nleft > 0) {
		if ( (nwritten = write(fd, ptr, nleft)) <= 0) {
			if (nwritten < 0 && errno == EINTR) {
                                /* If interrupted, try again */
				nwritten = 0;
                        }
			else
				return -1;
		}

		nleft -= nwritten;
		ptr   += nwritten;
	}
	return n;
}
