#ifndef MAIL_FRONT__SASL_AUTH__H__
#define MAIL_FRONT__SASL_AUTH__H__

struct str;
extern int sasl_auth_init(void);
extern int sasl_auth_cap(struct str* line);
extern int sasl_auth(const char* prefix, const struct str* arg);
extern const char* sasl_auth_msg(int* code);

#endif
