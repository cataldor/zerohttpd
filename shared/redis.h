#ifndef __REDIS_H__
#define __REDIS_H__

#include <stdint.h>
#include <sys/queue.h>

#define REDIS_SERVER_HOST	"127.0.0.1"
#define REDIS_SERVER_PORT	6379

#define RAE_TYPE_INTEGER	':'
#define RAE_TYPE_ERROR		'-'
#define RAE_TYPE_STRING		'$'
struct redis_array_elm {
	int rae_type;
	union {
		/* redis integer value cannot be larger than 512M */
		uint32_t rae_int;
		uint32_t rae_err;
		char *rae_string;
	};
	SIMPLEQ_ENTRY(redis_array_elm)	rae_next;
};

SIMPLEQ_HEAD(redis_array, rae_next);

int
redis_connect_server(const char *redis_ip, uint16_t redis_port);

int
redis_request_array(const char *key, uint32_t start, uint32_t end,
                    struct redis_array **ra, int sock);

int
_redis_request_array_cmd(const char *key, uint32_t start, uint32_t end,
			 char **buf);

int
_redis_request_array_reply(const char *key, uint32_t start, uint32_t end,
			   struct redis_array **ra, int sock);

#endif
