#ifndef MAIL_FRONT__SMTP__H__
#define MAIL_FRONT__SMTP__H__

#include "iobuf/iobuf.h"
#include "str/str.h"

extern unsigned long maxdatabytes;
extern unsigned maxhops;
extern str line;
extern str domain_name;
extern const char UNKNOWN[];

#define smtp_get_line() (ibuf_getstr_crlf(&inbuf, &line))
extern int smtp_mainloop(const char* welcome);
extern int smtp_dispatch(void);

#endif
