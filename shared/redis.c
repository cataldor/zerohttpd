#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>

#include "redis.h"

static size_t
__max_request_array_cmd(const char *key, uint32_t start, uint32_t end)
{
	const size_t max_int_chars = 11;
	/* 4, $6, LRANGE */
	const size_t fixed_values = 9;
	const size_t elm_end = 2;
	const size_t max_int_len = 3;
	const size_t cmd_size = fixed_values + (max_int_chars * 2) +
				(max_int_len * 2) + (elm_end * 9) +
				(strlen(key) * 2);

	return (cmd_size);
}

static inline int
__intlen(size_t number)
{
	return (snprintf(NULL, 0, "%zu", number));
}

int
_redis_request_array_cmd(const char *key, uint32_t start, uint32_t end,
			 char **buf)
{
	int ret;
	const size_t cmd_size = __max_request_array_cmd(key, start, end);

	*buf = malloc(cmd_size);
	if (*buf == NULL)
		return (-1);

	ret = snprintf(*buf, cmd_size,
		       "*4\r\n" "$6\r\nLRANGE\r\n"
		       "%zu\r\n%s\r\n"
		       "%d\r\n%u\r\n" "%d\r\n%u\r\n",
		       strlen(key), key,
		       __intlen(start), start, __intlen(end), end);
	if (ret < 0 || (size_t)ret >= cmd_size)
		return (-1);
	return (0);
}

static int
__read_expected(int *out, int expected, int sock)
{
	ssize_t ret;

	ret = read(sock, out, 1);
	if (ret < 0)
		return ((int)ret);
	if (*out != expected)
		warnx("read: expected 0x%x got 0x%x\n", *out, expected);

	return (0);
}

static int
__sock_ascii_to_num(size_t *out, int sock)
{
	int ch;
	ssize_t ret;
	size_t items = 0;

	while (1) {
		ret = read(sock, &ch, 1);
		if (ret < 0)
			return (-1);
		/* end of items */
		if (ch == '\r') {
			/* read next \n */
			ret = __read_expected(&ch, '\n', sock);
			if (ret < 0)
				return (-1);
		}
		items = (items * 10) + (ch - '0');
	}
	*out = items;
	return (0);
}

int
_redis_request_array_reply(const char *key, uint32_t start, uint32_t end,
			   struct redis_array **ra, int sock)
{
	int ch;
	ssize_t ret;
	size_t i;
	size_t ra_items;

	/* expected reply is an array */
	ret = read(sock, &ch, 1);
	if (ret < 0)
		return (-1);
	if (ch != '*')
		return (-1);

	/* number of items */
	ret = __sock_ascii_to_num(&ra_items, sock);
	if (ret < 0)
		return (-1);

	*ra = calloc(ra_items, sizeof(**ra));
	if (*ra == NULL)
		return (-1);

	for (i = 0; i < ra_items; i++) {
		/* read $ */
		ret = read(sock, &ch, 1);
		if (ch != '$')
			warnx("read: expected 0x%x got 0x%x\n", ch, '\n');
		/* XXX continue */
	}
	return (0);
}

/*
 * Fetch a range of items in a list from start to end.
 *
 * The list and its elements are dynamically allocated.
 *
 * The Redis protocol for arrays:
 * 
 * - Request
 *   *4\r\n $6\r\n LRANGE\r\n {strlen(key)}\r\n key\r\n {strlen(start)}\r\n
 *   start\r\n {strlen(end)}\r\n end\r\n
 *
 *   (spaces are for clarity only, not present in the protocol)
 *   * => array request
 *   CRLF => end of element
 *   4 => 4 RESP builk strings (lrange key start stop)
 *   $6 => 6 character string to follow
 *   LRANGE => string
 *   ...
 *
 */
int
redis_request_array(const char *key, uint32_t start, uint32_t end,
                    struct redis_array **ra, int sock)
{
	int ret;
	ssize_t ssret;
	char *cmd;

	ret = _redis_request_array_cmd(key, start, end, &cmd);
	if (ret < 0) {
		ret = -1;
		goto end;
	}

	ssret = write(sock, cmd, strlen(cmd));
	if (ssret != (ssize_t)strlen(cmd)) {
		ret = -1;
		goto end;
	}

	ret = _redis_request_array_reply(key, start, end, ra, sock);

end:
	free(cmd);
	return (ret);
}

int
redis_connect_server(const char *redis_ip, uint16_t redis_port)
{
	int ret;
	int redis_fd;
	struct sockaddr_in saddr;

	redis_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (redis_fd < 0)
		err(1, "socket");

	bzero(&saddr, sizeof(saddr));
	saddr.sin_family = AF_INET;
	saddr.sin_port = htons(redis_port);
	ret = inet_pton(AF_INET, redis_ip, &saddr.sin_addr);
	if (ret <= 0)
		err(1, "inet_pton");

	ret = connect(redis_fd, (struct sockaddr *)&saddr, sizeof(saddr));
	if (ret < 0)
		err(1, "connect");
	printf("connected to redis server %s@%d\n", redis_ip, redis_port);
	return (redis_fd);
}

