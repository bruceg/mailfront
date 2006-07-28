#include "mailfront.h"

const char program[] = "smtpfront-qmail";
const char default_plugins[] = "require-auth:check-fqdn:counters:mailrules:relayclient:qmail-validate:cvm-validate:add-received:patterns";

int main(void)
{
  return protocol_mainloop();
}
