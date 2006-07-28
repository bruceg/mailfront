#include "mailfront.h"

const char program[] = "qmtpfront-qmail";

int main(void)
{
  return protocol_mainloop();
}
