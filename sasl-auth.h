#ifndef MAIL_FRONT__SASL_AUTH__H__
#define MAIL_FRONT__SASL_AUTH__H__

struct str;
extern int sasl_auth_init(void);
extern int sasl_auth_cap(struct str* caps);
extern int sasl_auth1(const char* prefix, const struct str* arg);
extern int sasl_auth2(const char* prefix,
		      const char* mech, const char* iresponse);
extern const char* sasl_auth_msg(int* code);

#endif
