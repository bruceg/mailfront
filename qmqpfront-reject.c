#include "mailfront.h"
#include "qmtp.h"

const char program[] = "qmqpfront-reject";
const char default_plugins[] = "";

int mainloop(void) {
  return qmqp_mainloop();
}
