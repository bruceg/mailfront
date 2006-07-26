#include <stdlib.h>
#include "mailfront.h"
#include "qmtp.h"
#include <iobuf/iobuf.h>

const char program[] = "qmtpfront-qmail";
const char default_plugins[] = "require-auth:check-fqdn:counters:mailrules:relayclient:qmail-validate:cvm-validate:add-received:patterns";

int main(void)
{
  return qmtp_mainloop();
}
