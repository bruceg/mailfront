#include <stdlib.h>
#include "mailfront.h"
#include "smtp.h"
#include <iobuf/iobuf.h>

const char program[] = "smtpfront-qmail";
const char default_plugins[] = "require-auth:check-fqdn:counters:mailrules:relayclient:qmail-validate:cvm-validate:add-received:patterns";

int main(void)
{
  return smtp_mainloop();
}
