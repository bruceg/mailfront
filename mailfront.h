#ifndef MAIL_FRONT__MAILFRONT__H__
#define MAIL_FRONT__MAILFRONT__H__

#include "responses.h"
#include <str/str.h>
#include "constants.h"

/* From std-handle.c */
extern const char UNKNOWN[];
extern unsigned long maxdatabytes;
extern unsigned maxhops;
extern int authenticated;
extern const char* relayclient;
extern int number_ok(const response* resp);
extern int response_ok(const response* resp);
extern const response* std_handle_init(void);
extern const response* std_handle_reset(void);
extern const response* std_handle_sender(str* sender);
extern const response* std_handle_recipient(str* recip);
extern const response* std_handle_data_start(const str* helo_domain,
					     const char* protocol);
extern void std_handle_data_bytes(const char* bytes, unsigned len);
extern const response* std_handle_data_end(void);

/* Defined by a back-end module */
extern const response* validate_sender(str*);
extern const response* validate_recipient(str*);
extern void handle_reset(void);
extern const response* handle_sender(str*);
extern const response* handle_recipient(str*);
extern const response* handle_data_start(void);
extern void handle_data_bytes(const char*, unsigned long);
extern const response* handle_data_end(void);

#endif /* MAIL_FRONT__MAILFRONT__H__ */
