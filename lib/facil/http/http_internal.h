/*
Copyright: Boaz segev, 2016-2017
License: MIT

Feel free to copy, use and enjoy according to the license provided.
*/
#ifndef H_HTTP_INTERNAL_H
#define H_HTTP_INTERNAL_H

#include "http.h"

#include <arpa/inet.h>
#include <errno.h>

/* *****************************************************************************
Types
***************************************************************************** */

typedef struct http_protocol_s http_protocol_s;
typedef struct http_vtable_s http_vtable_s;

struct http_vtable_s {
  /** Should send existing headers and data */
  int (*const http_send_body)(http_s *h, void *data, uintptr_t length);
  /** Should send existing headers and file */
  int (*const http_sendfile)(http_s *h, int fd, uintptr_t length,
                             uintptr_t offset);
  /** Should send existing headers and data and prepare for streaming */
  int (*const http_stream)(http_s *h, void *data, uintptr_t length);
  /** Should send existing headers or complete streaming */
  void (*const http_finish)(http_s *h);
  /** Push for data. */
  int (*const http_push_data)(http_s *h, void *data, uintptr_t length,
                              fiobj_s *mime_type);
  /** Push for files. */
  int (*const http_push_file)(http_s *h, fiobj_s *filename, fiobj_s *mime_type);
  /** Defer request handling for later... careful (memory concern apply). */
  int (*const http_defer)(http_s *h, void (*task)(http_s *h),
                          void (*fallback)(http_s *h));
  /** Defer request handling for later... careful (memory concern apply). */
  void (*const http2websocket)(websocket_settings_s *arg);
};

struct http_protocol_s {
  protocol_s protocol;
  intptr_t uuid;
  http_settings_s *settings;
  http_vtable_s *vtable;
};

#define http2protocol(h) ((http_protocol_s *)h->private_data.owner)

/* *****************************************************************************
Constants that shouldn't be accessed by the users (`fiobj_dup` required).
*****************************************************************************
*/

extern fiobj_s *HTTP_HEADER_ACCEPT_RANGES;
extern fiobj_s *HTTP_HEADER_WS_SEC_KEY;
extern fiobj_s *HTTP_HVALUE_BYTES;
extern fiobj_s *HTTP_HVALUE_CLOSE;
extern fiobj_s *HTTP_HVALUE_GZIP;
extern fiobj_s *HTTP_HVALUE_KEEP_ALIVE;
extern fiobj_s *HTTP_HVALUE_MAX_AGE;
extern fiobj_s *HTTP_HVALUE_WEBSOCKET;
extern fiobj_s *HTTP_HVALUE_WS_SEC_VERSION;
extern fiobj_s *HTTP_HVALUE_WS_UPGRADE;
extern fiobj_s *HTTP_HVALUE_WS_VERSION;

/* *****************************************************************************
HTTP request/response object management
*****************************************************************************
*/

static inline void http_s_init(http_s *h, http_protocol_s *owner) {
  *h = (http_s){
      .private_data.owner = (protocol_s *)owner,
      .private_data.request_id = 1,
      .private_data.out_headers = fiobj_hash_new(),
      .headers = fiobj_hash_new(),
      .version = h->version,
      .received_at = facil_last_tick(),
      .status = 200,
      .udata = owner->settings->udata,
  };
}

static inline void http_s_cleanup(http_s *h) {
  if (h->status && ((http_protocol_s *)h->private_data.owner)->settings->log)
    http_write_log(h);
  fiobj_free(h->method); /* union for fiobj_free(r->status_str); */
  fiobj_free(h->private_data.out_headers);
  fiobj_free(h->headers);
  fiobj_free(h->version);
  fiobj_free(h->query);
  fiobj_free(h->path);
  fiobj_free(h->cookies);
  fiobj_free(h->body);
  fiobj_free(h->params);
  *h = (http_s){{0}};
}

/** Use this function to handle HTTP requests.*/
void http_on_request_handler______internal(http_s *h,
                                           http_settings_s *settings);

int http_send_error2(size_t error, intptr_t uuid, http_settings_s *settings);

/* *****************************************************************************
Helpers
***************************************************************************** */

#define HTTP_ASSERT(x, m)                                                      \
  if (!x)                                                                      \
    perror("FATAL ERROR: (http)" m), exit(errno);

/** send a fiobj_s * object through a socket. */
static inline __attribute__((unused)) int fiobj_send(intptr_t uuid,
                                                     fiobj_s *o) {
  fio_cstr_s s = fiobj_obj2cstr(o);
  // fprintf(stderr, "%s\n", s.data);
  return sock_write2(.uuid = uuid, .buffer = (o),
                     .offset = (((intptr_t)s.data) - ((intptr_t)(o))),
                     .length = s.length,
                     .dealloc = (void (*)(void *))fiobj_free);
}

/** sets an outgoing header only if it doesn't exist */
static inline void set_header_if_missing(fiobj_s *hash, fiobj_s *name,
                                         fiobj_s *value) {
  fiobj_s *old = fiobj_hash_replace(hash, name, value);
  if (!old)
    return;
  fiobj_hash_replace(hash, name, old);
  fiobj_free(value);
}

/** sets an outgoing header, collecting duplicates in an Array (i.e. cookies)
 */
static inline void set_header_add(fiobj_s *hash, fiobj_s *name,
                                  fiobj_s *value) {
  fiobj_s *old = fiobj_hash_replace(hash, name, value);
  if (!old)
    return;
  if (!value) {
    fiobj_free(old);
    return;
  }
  if (old->type != FIOBJ_T_ARRAY) {
    fiobj_s *tmp = fiobj_ary_new();
    fiobj_ary_push(tmp, old);
    old = tmp;
  }
  fiobj_ary_push(old, value);
  fiobj_hash_replace(hash, name, old);
}

#endif /* H_HTTP_INTERNAL_H */
