#include "mailfront.h"

const char program[] = "smtpfront-qmail";

int main(void)
{
  return protocol_mainloop();
}
