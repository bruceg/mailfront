#ifndef MAIL_FRONT__MAILFRONT__H__
#define MAIL_FRONT__MAILFRONT__H__

#include "responses.h"
#include <iobuf/iobuf.h>
#include <str/str.h>
#include "constants.h"

struct session
{
  const char* protocol;
  const char* helo_domain;
  const char* relayclient;
  int authenticated;
  unsigned long maxdatabytes;
  unsigned int maxhops;
  unsigned int maxrcpts;
};

struct module
{
  struct module* next;
  const response* (*init)(struct module*, struct session*);
  const response* (*reset)(struct module*, struct session*);
  const response* (*sender)(struct module*, struct session*,
			    str*);
  const response* (*recipient)(struct module*, struct session*,
			       str*);
  const response* (*data_start)(struct module*, struct session*);
  const response* (*data_block)(struct module*, struct session*,
				const char* bytes, unsigned long len);
  const response* (*data_end)(struct module*, struct session*);
};

/* From std-handle.c */
extern const char UNKNOWN[];
extern int number_ok(const response* resp);
extern int response_ok(const response* resp);
extern const response* handle_init(struct session* session);
extern const response* handle_reset(struct session* session);
extern const response* handle_sender(struct session* session, str* sender);
extern const response* handle_recipient(struct session* session, str* recip);
extern const response* handle_data_start(struct session* session);
extern void handle_data_bytes(struct session* session,
			      const char* bytes, unsigned len);
extern const response* handle_data_end(struct session* session);
extern void add_module(struct module*);

/* From cvm-validate.c */
extern const response* cvm_validate_init(void);
extern const response* cvm_validate_recipient(const str*);

/* From netstring.c */
int get_netstring_len(ibuf* in, unsigned long* i);
int get_netstring(ibuf* in, str* s);

/* From patterns.c */
void patterns_init(void);
const response* patterns_check(const char* block, unsigned len);

/* Defined by a back-end module */
extern const response* backend_validate_init(void);
extern const response* backend_validate_sender(str*);
extern const response* backend_validate_recipient(str*);
extern void backend_handle_reset(struct session*);
extern const response* backend_handle_sender(str*);
extern const response* backend_handle_recipient(str*);
extern const response* backend_handle_data_start(void);
extern void backend_handle_data_bytes(const char*, unsigned long);
extern const response* backend_handle_data_end(void);

#endif /* MAIL_FRONT__MAILFRONT__H__ */
