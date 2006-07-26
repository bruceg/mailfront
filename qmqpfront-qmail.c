#include <stdlib.h>
#include "mailfront.h"
#include <iobuf/iobuf.h>

const char program[] = "qmqpfront-qmail";
const char default_plugins[] = "require-auth:check-fqdn:counters:mailrules:relayclient:qmail-validate:cvm-validate:add-received:patterns";

extern int qmqp_mainloop(void);

int main(void)
{
  return qmqp_mainloop();
}
