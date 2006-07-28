#include "mailfront.h"

const char program[] = "qmtpfront-echo";

int main(void)
{
  return protocol_mainloop();
}
