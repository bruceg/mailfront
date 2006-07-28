#include "mailfront.h"

const char program[] = "qmtpfront-echo";
const char default_plugins[] = "";

int main(void)
{
  return protocol_mainloop();
}
