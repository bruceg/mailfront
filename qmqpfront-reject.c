#include "mailfront.h"
#include "qmtp.h"

const char program[] = "qmqpfront-reject";

int mainloop(void) {
  return qmqp_mainloop();
}
