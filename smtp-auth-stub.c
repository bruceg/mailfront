#include "mailfront.h"
#include "smtp.h"

int smtp_auth_init(void) { return 1; }
int smtp_auth_cap(str* line) { return 0; }
int smtp_auth(str* args) { return respond(500, 1, "Not implemented."); }
