#ifndef MAIL_FRONT__SMTP__H__
#define MAIL_FRONT__SMTP__H__

#define CR ((char)13)
#define LF ((char)10)
#define SPACE ((char)32)
#define TAB ((char)9)
#define COLON ((char)':')
#define LBRACE ((char)'<')
#define RBRACE ((char)'>')
#define PERIOD ((char)'.')

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

extern int smtp_auth_init(void);
extern int smtp_auth_cap(str*);
extern int smtp_auth(str*);

#endif
