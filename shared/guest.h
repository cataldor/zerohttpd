#ifndef __GUEST_H__
#define __GUEST_H__

#include "http.h"

#define GUESTBOOK_ROUTE			"/guestbook"
#define GUESTBOOK_TEMPLATE		"template/guestbook/index.html"
#define GUESTBOOK_REDIS_VISITOR_KEY	"visitor_count"
#define GUESTBOOK_REDIS_REMARKS_KEY	"guestbook_remarks"
#define GUESTBOOK_TEMPLATE_VISITOR	"$VISITOR_COUNT$"
#define GUESTBOOK_TEMPATE_REMARKS	"$GUEST_REMARKS$"

int
guest_handle_get_method(enum http_dynamic *hd, const char *path, int sock);

int
_guest_handle_get_route(const char *path, int sock);

#endif
