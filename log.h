#ifndef MAIL_FRONT__LOG__H__
#define MAIL_FRONT__LOG__H__

extern const char program[];
extern void log_start(void);
extern void log_end(void);
extern void log_s(const char*);
extern void log_u(unsigned long);
extern void log1(const char*);

#endif
