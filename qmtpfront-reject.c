#include "mailfront.h"

const char program[] = "qmtpfront-reject";

int mainloop(void) {
  return protocol_mainloop();
}
