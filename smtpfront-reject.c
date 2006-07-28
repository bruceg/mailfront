#include "mailfront.h"

const char program[] = "smtpfront-reject";

int mainloop(void) {
  return protocol_mainloop();
}
