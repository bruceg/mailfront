#include "mailfront.h"
#include "smtp.h"

const char program[] = "smtpfront-echo";
const char default_plugins[] = "";

int main(void)
{
  return smtp_mainloop();
}
