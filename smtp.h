#ifndef MAIL_FRONT__SMTP__H__
#define MAIL_FRONT__SMTP__H__

#include "responses.h"
#include <iobuf/iobuf.h>
#include <str/str.h>

extern unsigned long maxdatabytes;
extern unsigned maxhops;
extern str line;
extern str domain_name;
extern const char UNKNOWN[];

#define smtp_get_line() (ibuf_getstr_crlf(&inbuf, &line))
extern int smtp_mainloop(void);
extern int smtp_dispatch(void);

/* State variables */
extern int authenticated;
extern const char* relayclient;

extern int respond(unsigned number, int final, const char* msg);
extern int respond_start(unsigned number, int nonfinal);
extern int respond_str(const char* msg);
extern int respond_end(void);
extern int respond_resp(const response* resp, int final);

#endif
