#include "mailfront.h"
#include "smtp.h"

const char program[] = "smtpfront-reject";
const char default_plugins[] = "";

int mainloop(void) {
  return smtp_mainloop();
}
