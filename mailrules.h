#ifndef MAIL_FRONT__MAIL_RULES__H__
#define MAIL_FRONT__MAIL_RULES__H__

extern const response* rules_add(const char* line);
extern const response* rules_init(void);
extern const response* rules_reset(void);
extern const char* rules_getenv(const char* name);
extern int rules_exportenv(void);
extern const response* rules_validate_sender(str*);
extern const response* rules_validate_recipient(str*);

#endif
