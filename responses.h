#ifndef MAIL_FRONT__RESPONSES__H__
#define MAIL_FRONT__RESPONSES__H__

struct response 
{
  const struct response* prev;
  unsigned number;
  const char* message;
};
typedef struct response response;

extern const response resp_reject_domain;
#define RESPONSE(NAME,CODE,MSG) const response resp_##NAME = {0,CODE,MSG}

#endif
