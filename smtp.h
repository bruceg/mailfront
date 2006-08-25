#ifndef MAIL_FRONT__SMTP__H__
#define MAIL_FRONT__SMTP__H__

#include "responses.h"

extern int smtp_respond_part(unsigned number, int final, const char* msg);
extern int smtp_respond(const response* resp);

#endif
