#ifndef MAIL_FRONT__QMAIL__H__
#define MAIL_FRONT__QMAIL__H__

extern const response* qmail_validate_init(void);
extern const response* qmail_validate_sender(const str*);
extern const response* qmail_validate_recipient(const str*);

extern void qmail_reset(void);
extern const response* qmail_sender(const str*);
extern const response* qmail_recipient(const str*);
extern const response* qmail_data_start(void);
extern void qmail_data_bytes(const char*, unsigned long);
extern const response* qmail_data_end(void);

#endif
