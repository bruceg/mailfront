#include <string.h>
#include <stdlib.h>
#include <systime.h>

#include "mailfront.h"
#include "mailrules.h"
#include "smtp.h"
#include "sasl-auth.h"

#include <iobuf/iobuf.h>
#include <msg/msg.h>

str line = {0,0,0};
str domain_name = {0,0,0};

extern unsigned long maxnotimpl;

int smtp_mainloop(void)
{
  static str str_welcome;
  const char* tmp;
  const response* resp;

  if ((tmp = getenv("TCPLOCALHOST")) == 0) tmp = UNKNOWN;
  str_copys(&domain_name, tmp);

  if ((tmp = getenv("SMTPGREETING")) != 0)
    str_copys(&str_welcome, tmp);
  else {
    str_copy(&str_welcome, &domain_name);
    str_cats(&str_welcome, " mailfront");
  }
  str_cats(&str_welcome, " ESMTP");

  if ((tmp = getenv("MAXNOTIMPL")) != 0)
    maxnotimpl = strtoul(tmp, 0, 10);

  if ((resp = handle_init()) != 0) { respond_resp(resp, 1); return 1; }

  if (!sasl_auth_init()) return respond(421, 1, "Failed to initialize AUTH");

  if (!respond(220, 1, str_welcome.s)) return 1;
  while (smtp_get_line())
    if (!smtp_dispatch()) {
      if (ibuf_eof(&inbuf))
	msg1("Connection dropped");
      if (ibuf_timedout(&inbuf))
	msg1("Timed out");
      return 1;
    }
  return 0;
}
