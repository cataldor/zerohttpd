#ifndef __SHARED_H__
#define __SHARED_H__

#include <sys/stat.h>
#include <sys/resource.h>

#include <err.h>
#include <signal.h>
#include <stdio.h>
#include <unistd.h>

#define	SERVER_STRING		"Server: zerohttpd/1\r\n"
#define	DEFAULT_SERVER_PORT	8000

extern volatile sig_atomic_t exit_requested;

/*
 * Check if a signal was received.
 * If a signal was receive, print the current resouce usage.
 *
 * This function is not intended to be async-safe.
 *
 * Return 1 if a signal was received, 0 if not.
 */
static inline
int exit_requested_check(void)
{
	int ret;
	struct rusage ru;

	if (exit_requested == 0)
		return (0);
	ret = getrusage(RUSAGE_SELF, &ru);
	if (ret < 0)
		err(1, "getrusage");

	printf("\nUser time: %lds %ldms, System time: %lds %ldms\n",
		ru.ru_utime.tv_sec, ru.ru_utime.tv_usec/1000,
		ru.ru_stime.tv_sec, ru.ru_stime.tv_usec/1000);
	return (1);
}

static inline int
file_size(int fd, size_t *fsize)
{
	int ret;
	struct stat st;

	ret = fstat(fd, &st);
	if (ret < 0)
		return -1;
	*fsize = st.st_size;
	return (0);
}

#endif
