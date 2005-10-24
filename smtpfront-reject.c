#include "mailfront.h"
#include "smtp.h"

const char program[] = "smtpfront-reject";

int mainloop(void) {
  return smtp_mainloop();
}
