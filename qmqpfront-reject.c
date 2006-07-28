#include "mailfront.h"

const char program[] = "qmqpfront-reject";

int mainloop(void) {
  return protocol_mainloop();
}
