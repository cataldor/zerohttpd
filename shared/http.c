#include <sys/sendfile.h>
#include <sys/socket.h>
#include <sys/stat.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "../shared.h"
#include "guest.h"
#include "http.h"

static const char * const http_404_content = \
	"HTTP/1.0 404 Not Found\r\n"
	"Content-type: text/html\r\n"
	"\r\n"
	"<html>"
	"<head>"
	"<title>zerohttpd: not found</title>"
	"</head>"
	"<body>"
	"<h1>Not Found (404)</h1>"
	"<p> Your client is asking for an object that was not found "
	"on this server.</p>"
	"</body>"
	"</html>";

static const char * const http_400_content = \
	"HTTP/1.0 400 Bad Request\r\n"
	"Content-type: text/html\r\n"
	"\r\n"
	"<html>"
	"<head>"
	"<title>zerohttpd: unimplemented</title>"
	"</head>"
	"<body>"
	"<h1>Bad Request (unimplemented)</h1>"
	"<p>Your client sent a request zerohttpd did not understand "
	"and it is probably not your fault.</p>"
	"</body>"
	"</html>";

/*
 * read up to one line (\r or \r\n) and insert it into buf
 *
 * returns the number of characters read. if no space was available for the
 * full line, a NUL character is added at the last position of the truncated
 * line
 */
size_t
http_line(int sock, char *buf, size_t size)
{
	ssize_t n;
	size_t pos = 0;
	char c = '\0';

	/* very simple; not super efficient */
	while (c != '\n' && pos < size - 2) {
		n = recv(sock, &c, 1, 0);
		if (n < 0)
			err(1, "recv");
		if (n == 0)
			return 0;

		if (c == '\r') {
			/* consume '\n' if it exists */
			n = recv(sock, &c, 1, MSG_PEEK);
			if (n > 0 && c == '\n')
				n = recv(sock, &c, 1, 0);
			if (n < 0)
				err(1, "recv");
		} else {
			buf[pos] = c;
			pos++;
		}
	}
	buf[pos] = '\0';
	return (pos);
}

/*
 * send a 404 page to the socket sock
 */
void
_http_send_404(int sock)
{
	ssize_t ssret;

	ssret = send(sock, http_404_content, strlen(http_404_content), 0);
	if ((size_t)ssret != strlen(http_404_content))
		err(1, "send");
}

int
_http_handle_get_dynamic_page(enum http_dynamic *hd, const char *path, int sock)
{
	return guest_handle_get_method(hd, path, sock);
}

/*
 * transform the extension of the file in path to a content type string from
 * http/1
 */
 static const char *
content_type_string(const char *path)
{
	const char *dot = strrchr(path, '.');

	if (dot == NULL || dot == path)
		return "application/octet-stream";

	dot++;
	if (strcmp("jpg", dot) == 0 || strcmp("jpeg", dot) == 0)
		return "image/jpeg";
	if (strcmp("png", dot) == 0)
		return "image/png";
	if (strcmp("gif", dot) == 0)
		return "image/gif";
	if (strcmp("htm", dot) == 0 || strcmp("html", dot) == 0)
		return "text/html";
	if (strcmp("js", dot) == 0)
		return "application/javascript";
	if (strcmp("css", dot) == 0)
		return "text/css";
	if (strcmp("txt", dot) == 0)
		return "text/plain";
	else
		return "application/octet-stream";
}

/*
 * build the string for the 200 OK header in buf (up to buflen characters)
 *
 * path and len are the location and size of the file asked.
 *
 * returns 0 for success, -1 for insufficient buflen
 */
static int
build_ok_header(const char *path, off_t len, char *buf, size_t buflen)
{
	int ret;

	/* a line containing the '\r\n' sequence terminates the header */
	ret = snprintf(buf, buflen,
		       "HTTP/1.0 200 OK\r\n"
		       "%s"
		       "Content-Type: %s\r\n"
		       "Content-Length: %zu\r\n"
		       "Location: %s\r\n"
		       "\r\n",
		       SERVER_STRING, content_type_string(path), len, path);
	if ((size_t)ret >= buflen) {
		errno = ENOMEM;
		return -1;
	}
	return 0;
}

/*
 * send the OK header string in socket sock
 */
static void
send_ok_header(const char *path, off_t len, int sock)
{
	int ret;
	ssize_t ssret;
	char buf[128];

	ret = build_ok_header(path, len, buf, sizeof(buf));
	if (ret < 0)
		err(1, "build_ok_header");

	ssret = send(sock, buf, strlen(buf), 0);
	if ((size_t)ssret != strlen(buf))
		err(1, "send");
}

/*
 * send a file to the socket sock
 */
static void
send_file_sock(const char *path, off_t file_size, int sock)
{
	int fd;
	ssize_t ret;

	fd = open(path, O_RDONLY);
	if (fd < 0)
		err(1, "open");
	ret = sendfile(sock, fd, NULL, file_size);
	if (ret < 0)
		err(1, "sendfile");
}

void
_http_handle_get_method(const char *path, int sock)
{
	int ret;
	enum http_dynamic hd;
	char *full_path;
	struct stat full_path_stat;
	const size_t worst_case_size = strlen("public") + strlen(path) +
				       strlen("index.html") + 1;

	ret = _http_handle_get_dynamic_page(&hd, path, sock);	
	if (ret < 0)
		err(1, "_http_handle_get_dynamic_page");

	/* handled already */
	if (hd != HTTP_DYNAMIC_PAGE_NOTFOUND)
		return;

	/* static pages handling */
	full_path = malloc(worst_case_size);
	if (full_path == NULL)
		err(1, "malloc");

	/* assume the client wants index if it is a trailing slash */
	if (path[strlen(path) - 1] == '/')
		ret = snprintf(full_path, worst_case_size, "public%sindex.html",
			       path);
	else
		ret = snprintf(full_path, worst_case_size, "public%s", path);

	if ((size_t)ret >= worst_case_size)
		err(1, "string overflow");

	/* what is the size ? */
	ret = stat(full_path, &full_path_stat);
	if (ret < 0 && errno == ENOENT)
		goto notfound;
	if (ret < 0)
		err(1, "stat");

	/* stat successful */
	/* make sure it is a file and not some random object */
	if (S_ISREG(full_path_stat.st_mode) == 0)
		goto notfound;

	/* it is a file, nice */
	send_ok_header(full_path, full_path_stat.st_size, sock);
	send_file_sock(full_path, full_path_stat.st_size, sock);

	free(full_path);
	return;

notfound:
	printf("404 not found: %s\n", full_path);
	_http_send_404(sock);
	free(full_path);
}

/*
 * POST is used to send data to create/update a resource for the server.
 * For our sample, only guestbook receive resources, so we do not deal with the
 * static pages
 */
static void
_http_handle_post_method(const char *path, int sock)
{
}

/*
 * Any HTTP method that is not GET/POST is unimplemented in zerohttpd
 * Notify the client
 */
static void
_http_handle_unimpl_method(const char *path, int sock)
{
	ssize_t ssret;

	ssret = send(sock, http_400_content, strlen(http_400_content), 0);
	if ((size_t)ssret != strlen(http_400_content))
		err(1, "send");
}

static int
__is_get_method(const char *word)
{
	return strcmp(word, "GET") == 0;
}

static int
__is_post_method(const char *word)
{
	return strcmp(word, "POST") == 0;
}

void
http_handle_method(const char *line, int sock)
{
	char *dupline;
	char *method;
	char *path;

	dupline = strdup(line);
	method = strtok(dupline, " ");
	path = strtok(NULL, " ");

	if (__is_get_method(method))
		_http_handle_get_method(path, sock);
	else if (__is_post_method(method))
		_http_handle_post_method(path, sock);	
	else
		_http_handle_unimpl_method(path, sock);;

	free(dupline);
}
