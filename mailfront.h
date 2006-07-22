#ifndef MAIL_FRONT__MAILFRONT__H__
#define MAIL_FRONT__MAILFRONT__H__

#include "responses.h"
#include <iobuf/iobuf.h>
#include <str/str.h>
#include "constants.h"

struct session
{
  const char* protocol;
  const char* linkproto;
  const char* helo_domain;
  const char* relayclient;
  const char* local_host;
  const char* local_ip;
  const char* remote_host;
  const char* remote_ip;
  int authenticated;
  unsigned long maxdatabytes;
  unsigned int maxhops;
  unsigned int maxrcpts;
};

extern struct session session;

struct module
{
  struct module* next;
  const response* (*init)(void);
  const response* (*reset)(void);
  const response* (*sender)(str*);
  const response* (*recipient)(str*);
  const response* (*data_start)();
  const response* (*data_block)(const char* bytes, unsigned long len);
  const response* (*data_end)();
};

/* From std-handle.c */
extern const char UNKNOWN[];
extern int number_ok(const response* resp);
extern int response_ok(const response* resp);
extern const response* handle_init(void);
extern const response* handle_reset(void);
extern const response* handle_sender(str* sender);
extern const response* handle_recipient(str* recip);
extern const response* handle_data_start(void);
extern void handle_data_bytes(const char* bytes, unsigned len);
extern const response* handle_data_end(void);
extern void add_module(struct module*);

/* From netstring.c */
int get_netstring_len(ibuf* in, unsigned long* i);
int get_netstring(ibuf* in, str* s);

/* From patterns.c */
void patterns_init(void);
const response* patterns_check(const char* block, unsigned len);

/* Defined by a back-end module */
extern void backend_handle_reset();
extern const response* backend_handle_sender(str*);
extern const response* backend_handle_recipient(str*);
extern const response* backend_handle_data_start(void);
extern void backend_handle_data_bytes(const char*, unsigned long);
extern const response* backend_handle_data_end(void);

#endif /* MAIL_FRONT__MAILFRONT__H__ */
