#ifndef MAIL_FRONT__RESPONSES__H__
#define MAIL_FRONT__RESPONSES__H__

struct response 
{
  unsigned number;
  const char* message;
};
typedef struct response response;

extern const response resp_accepted_message;
extern const response resp_accepted_sender;
extern const response resp_accepted_recip;
extern const response resp_internal;
extern const response resp_oom;

#define RESPONSE(NAME,CODE,MSG) const response resp_##NAME = {CODE,MSG}

extern int number_ok(const response* r);
extern int response_ok(const response* r);

#endif
