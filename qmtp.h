#ifndef MAIL_FRONT__QMTP__H__
#define MAIL_FRONT__QMTP__H__

#include "responses.h"

extern int respond(unsigned number, int final, const char* msg);
extern int respond_resp(const response* resp);

#endif
