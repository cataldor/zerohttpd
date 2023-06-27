/* XXX temporary */
#include <sys/socket.h>

#include <fcntl.h>
#include <stdlib.h>
#include <string.h>

#include "guest.h"
#include "../shared.h"

int
_guest_handle_get_route(const char *path, int sock)
{
	int ret;
	int template_fd;
	size_t template_size;
	ssize_t ssret;
	char *template;

	template_fd = open(GUESTBOOK_TEMPLATE, O_RDONLY);
	if (template_fd < 0)
		return -1;

	ret = file_size(template_fd, &template_size);
	if (ret < 0)
		return -1;

	template = malloc(template_size + 1);
	if (template == NULL)
		return -1;

	ssret = read(template_fd, template, template_size);
	if (ssret != (ssize_t)template_size)
		warnx("read: expected %zu got %zd\n", template_size, ssret);

	/* XXX temporary */
	ssret = send(sock, template, template_size, 0);
	if (ssret < 0)
		warn("sendfile");

	ret = close(template_fd);
	if (ret < 0)
		warn("close");

	free(template);
	return (0);
}

int
guest_handle_get_method(enum http_dynamic *hd, const char *path, int sock)
{
	if (strcmp(path, GUESTBOOK_ROUTE) != 0) {
		*hd = HTTP_DYNAMIC_PAGE_NOTFOUND;
		return 0;
	}
		
	*hd = HTTP_DYNAMIC_PAGE_GUEST;
	return _guest_handle_get_route(path, sock);
}
