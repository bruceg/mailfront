#include "mailfront.h"
#include "smtp.h"

const char program[] = "smtpfront-echo";

int main(void)
{
  return smtp_mainloop();
}
