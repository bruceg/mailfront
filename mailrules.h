#ifndef MAIL_FRONT__MAIL_RULES__H__
#define MAIL_FRONT__MAIL_RULES__H__

extern void rules_add(const char* line);
extern const response* rules_init(void);
extern const response* rules_validate_sender(str*);
extern const response* rules_validate_recipient(str*);

#endif
