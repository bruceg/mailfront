#include <stdlib.h>
#include "mailfront.h"
#include "smtp.h"
#include <iobuf/iobuf.h>

const char program[] = "smtpfront-qmail";

int main(void)
{
  return smtp_mainloop();
}
