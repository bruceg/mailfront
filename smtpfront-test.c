#include "mailfront.h"
#include "smtp.h"

const char program[] = "smtpd-test";

int main(void)
{
  return smtp_mainloop(0);
}
