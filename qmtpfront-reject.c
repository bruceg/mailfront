#include "mailfront.h"
#include "qmtp.h"

const char program[] = "qmtpfront-reject";
const char default_plugins[] = "";

int mainloop(void) {
  return qmtp_mainloop();
}
