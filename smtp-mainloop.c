#include <string.h>
#include <stdlib.h>
#include <systime.h>

#include "mailfront.h"
#include "smtp.h"
#include <cvm/sasl.h>

#include <iobuf/iobuf.h>
#include <msg/msg.h>

static RESPONSE(authfail, 421, "4.3.0 Failed to initialize AUTH");

str line = {0,0,0};
str domain_name = {0,0,0};

struct sasl_auth saslauth = { .prefix = "334 " };

extern unsigned long maxnotimpl;

static str str_welcome;

int protocol_init(void)
{
  const char* tmp;

  session.protocol = "SMTP";
  
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

  return 0;
}

int protocol_mainloop(void)
{
  if (!sasl_auth_init(&saslauth))
    return respond_resp(&resp_authfail, 1);

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
