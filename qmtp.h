#ifndef MAIL_FRONT__QMTP__H__
#define MAIL_FRONT__QMTP__H__

#include "responses.h"

extern int qmtp_mainloop(void);

extern int respond(unsigned number, int final, const char* msg);
extern int respond_start(unsigned number, int nonfinal);
extern int respond_str(const char* msg);
extern int respond_end(void);
extern int respond_resp(const response* resp, int final);

#endif
