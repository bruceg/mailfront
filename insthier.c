#include "conf_bin.c"
#include "installer.h"

void insthier(void) {
  int bin = opendir(conf_bin);
  c(bin, "smtpfront-qmail", -1, -1, 0755);
  c(bin, "smtpfront-test",  -1, -1, 0755);
}
