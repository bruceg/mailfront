#ifndef MAIL_FRONT__MAILFRONT__H__
#define MAIL_FRONT__MAILFRONT__H__

#include "log.h"
#include "str/str.h"
#include "responses.h"

#define AT '@'

extern int respond(unsigned number, int final, const char* msg);
extern int respond_start(unsigned number, int nonfinal);
extern int respond_str(const char* msg);
extern int respond_end(void);
extern int respond_resp(const response* resp, int final);

/* Defined by a back-end module */
extern void handle_reset(void);
extern const response* handle_sender(str*);
extern const response* handle_recipient(str*);
extern const response* handle_data_start(void);
extern void handle_data_bytes(const char*, unsigned long);
extern const response* handle_data_end(void);

#endif /* MAIL_FRONT__MAILFRONT__H__ */
