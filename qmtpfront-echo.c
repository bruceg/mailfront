#include "mailfront.h"
#include "qmtp.h"

const char program[] = "qmtpfront-echo";
const char default_plugins[] = "";

int main(void)
{
  return qmtp_mainloop();
}
