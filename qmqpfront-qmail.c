#include "mailfront.h"

const char program[] = "qmqpfront-qmail";

int main(void)
{
  return protocol_mainloop();
}
