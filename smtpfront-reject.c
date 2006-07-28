#include "mailfront.h"

const char program[] = "smtpfront-reject";
const char default_plugins[] = "";

int mainloop(void) {
  return protocol_mainloop();
}
