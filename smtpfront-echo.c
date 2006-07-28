#include "mailfront.h"

const char program[] = "smtpfront-echo";

int main(void)
{
  return protocol_mainloop();
}
