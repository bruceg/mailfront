#ifndef MAIL_FRONT__MAILFRONT__H__
#define MAIL_FRONT__MAILFRONT__H__

#include "responses.h"
#include <str/str.h>
#include "constants.h"

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
