#include <stdlib.h>
#include "mailfront.h"
#include <iobuf/iobuf.h>

const char program[] = "qmqpfront-qmail";

extern int qmqp_mainloop(void);

int main(void)
{
  return qmqp_mainloop();
}
