#include "mailfront.h"
#include "qmtp.h"

const char program[] = "qmtpfront-echo";

int main(void)
{
  return qmtp_mainloop();
}
