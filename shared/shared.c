
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include <bsd/stdlib.h>

#include <limits.h>
#include <stdint.h>
#include <strings.h>

#include "../shared.h"
#include "redis.h"

extern void enter_loop(int server_socket, int redis_socket);

volatile sig_atomic_t exit_requested = 0;

static int
setup_server_socket(uint16_t port)
{
	int fd;
	int ret;
	struct sockaddr_in saddr;
	const int enable = 1;

	fd = socket(PF_INET, SOCK_STREAM, 0);
	if (fd < 0)
		err(1, "socket");

	ret = setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(enable));
	if (ret < 0)
		err(1, "setsockopt");

	bzero(&saddr, sizeof(saddr));
	saddr.sin_family = AF_INET;
	saddr.sin_port = htons(port);
	saddr.sin_addr.s_addr = htonl(INADDR_ANY);

	ret = bind(fd, (const struct sockaddr *)&saddr, sizeof(saddr));
	if (ret < 0)
		err(1, "bind");

	ret = listen(fd, 10);
	if (ret < 0)
		err(1, "listen");

	return (fd);
}

static void
exit_requested_signaled(int signo)
{
	exit_requested = 1;
}

void
__attribute__((weak))
enter_loop(int server_socket, int redis_socket)
{
	printf("missing %s\n", __func__);
}

int
main(int argc, char **argv)
{
	uint16_t	server_port;
	int	server_socket;
	int	redis_socket;
	int	ret;
	struct	sigaction	sa;
	const char	*redis_ip;
	const char	*errstr;

	if (argc > 1) {
		server_port = strtonum(argv[1], 1, UINT16_MAX, &errstr);
		if (errstr != NULL)
			err(1, "strtonum: %s:", errstr);
	} else
		server_port = DEFAULT_SERVER_PORT;

	if (argc > 2)
		redis_ip = argv[2];
	else
		redis_ip = REDIS_SERVER_HOST;

	server_socket = setup_server_socket(server_port);
	redis_socket = redis_connect_server(redis_ip, REDIS_SERVER_PORT);

	sa.sa_handler = exit_requested_signaled;
	sa.sa_flags = 0;
	ret = sigemptyset(&sa.sa_mask);
	if (ret < 0)
		err(1, "sigemptyset");

	ret = sigaction(SIGINT, &sa, NULL);
	if (ret < 0)
		err(1, "sigaction");

	printf("zerohttpd server listening on port %d\n", server_port);
	/* these will come in each sample */
	enter_loop(server_socket, redis_socket);

	return (0);
}
