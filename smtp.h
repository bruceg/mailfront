#ifndef MAIL_FRONT__SMTP__H__
#define MAIL_FRONT__SMTP__H__

#include "responses.h"
#include <iobuf/iobuf.h>
#include <str/str.h>

extern str line;
extern str domain_name;

struct sasl_auth;
extern struct sasl_auth saslauth;

#define smtp_get_line() (ibuf_getstr_crlf(&inbuf, &line))
extern int smtp_mainloop(void);
extern int smtp_dispatch(void);

extern int respond(unsigned number, int final, const char* msg);
extern int respond_resp(const response* resp, int final);

#endif
