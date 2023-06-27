#include <netinet/in.h>
#include <sys/socket.h>

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "../shared.h"
#include "../shared/http.h"

void
enter_loop(int server_socket, int redis_socket);

static void
handle_client(int client_socket)
{
	char *buf;
	char *http_method_line = NULL;
	size_t buf_len;
	size_t loops = 0;

	buf_len = 1024;
	buf = malloc(buf_len);
	if (buf == NULL)
		err(1, "malloc");

	while (1) {
		const size_t end = http_line(client_socket, buf, buf_len);

		if (loops == 0)
			http_method_line = strdup(buf);

		if (end == 0)
			break;
		printf("line %zu is: %s\n", loops, buf);
		loops++;
	}

	http_handle_method(http_method_line, client_socket);
	free(http_method_line);
	free(buf);
}

void
enter_loop(int server_socket, int redis_socket)
{
	struct sockaddr_in caddr;
	socklen_t caddr_len = sizeof(caddr);

	while (1) {
		int client_socket;

		if (exit_requested_check())
			break;
		client_socket = accept(server_socket, (struct sockaddr *)&caddr,
				       &caddr_len);
		if (client_socket < 0 && errno == EINTR)
			continue;
		if (client_socket < 0)
			err(1, "accept()");

		handle_client(client_socket);
		(void)close(client_socket);
	}
}
