#include <string.h>
#include <stdlib.h>
#include "systime.h"
#include "mailfront.h"
#include "smtp.h"
#include "sasl-auth.h"
#include "iobuf/iobuf.h"

str line = {0,0,0};
str domain_name = {0,0,0};
unsigned long maxdatabytes;
unsigned maxhops;

const char UNKNOWN[] = "unknown";

extern void report_io_bytes(void);

int smtp_mainloop(const char* welcome)
{
  static str str_welcome;
  
  unsigned long timeout;
  const char* tmp;

  atexit(report_io_bytes);
  
  timeout = 0;
  if ((tmp = getenv("TIMEOUT")) != 0) timeout = strtoul(tmp, 0, 10);
  if (timeout <= 0) timeout = 1200;
  inbuf.io.timeout = timeout * 1000;
  outbuf.io.timeout = timeout * 1000;

  maxhops = 0;
  if ((tmp = getenv("MAXHOPS")) != 0) maxhops = strtoul(tmp, 0, 10);
  if (maxhops == 0) maxhops = 100;
  
  if ((tmp = getenv("TCPLOCALHOST")) == 0) tmp = UNKNOWN;
  str_copys(&domain_name, tmp);
  str_copys(&str_welcome, tmp);
  str_cats(&str_welcome, " mailfront ESMTP");
  if (welcome) {
    str_catc(&str_welcome, ' ');
    str_cats(&str_welcome, welcome);
  }

  if ((tmp = getenv("DATABYTES")) != 0) maxdatabytes = strtoul(tmp, 0, 10);
  else maxdatabytes = 0;

  if (!sasl_auth_init()) return respond(421, 1, "Failed to initialize AUTH");

  if (!respond(220, 1, str_welcome.s)) return 1;
  while (smtp_get_line())
    if (!smtp_dispatch()) return 1;
  return 0;
}
