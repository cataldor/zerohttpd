#ifndef __HTTP_H__
#define __HTTP_H__

enum http_dynamic {
	HTTP_DYNAMIC_PAGE_NOTFOUND = 0,
	HTTP_DYNAMIC_PAGE_GUEST = 1,
};
	
/*
 * Read a http line on the tcp socket until a new-line is reached.
 * The new-line is not added to the buffer.
 *
 * return the last position written in buf
 */
size_t
http_line(int sock, char *buf, size_t size);

/*
 * Handle method request (GET/POST).
 *
 * Sends an error to the client if a handler is not found.
 */
void
http_handle_method(const char *line, int sock);

/*
 * send 404 page for the client attached to sock
 */
void
_http_send_404(int sock);

/*
 * handle a dynamic page in path, if it exists.
 * 
 * hd will be updated with the handled page; otherwise
 * HTTP_DYNAMIC_PAGE_NOTFOUND is written.
 */
int
_http_handle_get_dynamic_page(enum http_dynamic *hd, const char *path, int sock);

void
_http_handle_get_method(const char *path, int sock);

#endif
