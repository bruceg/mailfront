#include <string.h>
#include <stdlib.h>
#include <systime.h>
#include "mailfront.h"
#include "smtp.h"
#include "sasl-auth.h"
#include <iobuf/iobuf.h>

str line = {0,0,0};
str domain_name = {0,0,0};
unsigned long maxdatabytes;
unsigned maxhops;

const char UNKNOWN[] = "unknown";

extern void report_io_bytes(void);
extern void set_timeout(void);

int smtp_mainloop(void)
{
  static str str_welcome;
  const char* tmp;

  atexit(report_io_bytes);

  set_timeout();

  maxhops = 0;
  if ((tmp = getenv("MAXHOPS")) != 0) maxhops = strtoul(tmp, 0, 10);
  if (maxhops == 0) maxhops = 100;
  
  if ((tmp = getenv("SMTPGREETING")) != 0)
    str_copys(&str_welcome, tmp);
  else {
    if ((tmp = getenv("TCPLOCALHOST")) == 0) tmp = UNKNOWN;
    str_copys(&domain_name, tmp);
    str_copys(&str_welcome, tmp);
    str_cats(&str_welcome, " mailfront");
  }
  str_cats(&str_welcome, " ESMTP");

  if ((tmp = getenv("DATABYTES")) != 0) maxdatabytes = strtoul(tmp, 0, 10);
  else maxdatabytes = 0;

  if (!sasl_auth_init()) return respond(421, 1, "Failed to initialize AUTH");

  if (!respond(220, 1, str_welcome.s)) return 1;
  while (smtp_get_line())
    if (!smtp_dispatch()) return 1;
  return 0;
}
