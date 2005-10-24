#include "mailfront.h"
#include "qmtp.h"

const char program[] = "qmtpfront-reject";

int mainloop(void) {
  return qmtp_mainloop();
}
