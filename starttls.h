#ifndef MAILFRONT__STARTTLS__H__
#define MAILFRONT__STARTTLS__H__

#include "responses.h"

extern const response* starttls_init(void);
extern int starttls_start(void);
extern int starttls_available(void);
extern void starttls_disable(void);

#endif
