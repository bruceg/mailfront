#include "mailfront.h"

const char program[] = "smtpfront-echo";
const char default_plugins[] = "";

int main(void)
{
  return protocol_mainloop();
}
