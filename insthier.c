#include "conf_bin.c"
#include "installer.h"

void insthier(void) {
  int bin = opendir(conf_bin);
  c(bin, 0, "smtpfront-qmail", -1, -1, 0755);
  c(bin, 0, "smtpfront-test",  -1, -1, 0755);
}
