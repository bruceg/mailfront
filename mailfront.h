#ifndef MAIL_FRONT__MAILFRONT__H__
#define MAIL_FRONT__MAILFRONT__H__

#include "responses.h"
#include <iobuf/iobuf.h>
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
extern const response* handle_init(void);
extern const response* handle_reset(void);
extern const response* handle_sender(str* sender);
extern const response* handle_recipient(str* recip);
extern const response* handle_data_start(const char* helo_domain,
					     const char* protocol);
extern void handle_data_bytes(const char* bytes, unsigned len);
extern const response* handle_data_end(void);

/* From cvm-validate.c */
extern const response* cvm_validate_init(void);
extern const response* cvm_validate_recipient(const str*);

/* From msa.c */
extern const response* msa_validate_init(void);
extern const response* msa_validate_sender(str*);
extern const response* msa_validate_recipient(str*);

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
extern void backend_handle_reset(void);
extern const response* backend_handle_sender(str*);
extern const response* backend_handle_recipient(str*);
extern const response* backend_handle_data_start(void);
extern void backend_handle_data_bytes(const char*, unsigned long);
extern const response* backend_handle_data_end(void);

#endif /* MAIL_FRONT__MAILFRONT__H__ */
