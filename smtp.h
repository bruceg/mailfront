#ifndef MAIL_FRONT__SMTP__H__
#define MAIL_FRONT__SMTP__H__

#define CR 13
#define NL 10
#define SPACE ' '
#define TAB 9
#define COLON ':'
#define LBRACE '<'
#define RBRACE '>'
#define PERIOD '.'

#include "str/str.h"

extern unsigned long maxdatabytes;
extern unsigned maxhops;
extern str line;
extern str domain_name;
extern const char UNKNOWN[];

extern int smtp_mainloop(const char* welcome);
extern int smtp_dispatch(void);

#endif
