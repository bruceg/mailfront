#include <stdlib.h>
#include "mailfront.h"
#include "qmtp.h"
#include <iobuf/iobuf.h>

const char program[] = "qmtpfront-qmail";

int main(void)
{
  return qmtp_mainloop();
}
