#include "mailfront.h"
#include <string.h>
#include <cvm/facts.h>
#include <cvm/sasl.h>
#include <stdlib.h>

static RESPONSE(authfail, 421, "4.3.0 Failed to initialize AUTH");
static RESPONSE(auth_already, 503, "5.5.1 You are already authenticated.");
static RESPONSE(authenticated, 235, "2.7.0 Authentication succeeded.");

static struct sasl_auth saslauth = { .prefix = "334 " };

static str auth_caps;
static int require_tls;

static int enabled(void)
{
  return (sasl_mechanisms != 0)
    && (!require_tls
	|| (session_getnum("tls_state", 0) > 0));
}

static int cmd_AUTH(str* param)
{
  int i;
  if (session_getnum("authenticated", 0))
    return respond(&resp_auth_already);
  if ((i = sasl_auth1(&saslauth, param)) != 0) {
    const char* msg = sasl_auth_msg(&i);
    return respond_line(i, 1, msg, strlen(msg));
  }
  else {
    session_setnum("authenticated", 1);
    session_delstr("helo_domain");
    session_setstr("auth_user", cvm_fact_username);
    session_setnum("auth_uid", cvm_fact_userid);
    session_setnum("auth_gid", cvm_fact_groupid);
    if (cvm_fact_realname != 0)
      session_setstr("auth_realname", cvm_fact_realname);
    if (cvm_fact_domain != 0)
      session_setstr("auth_domain", cvm_fact_domain);
    if (cvm_fact_mailbox != 0)
      session_setstr("auth_mailbox", cvm_fact_mailbox);
    respond(&resp_authenticated);
  }
  return 1;
}

static struct command commands[] = {
  { "AUTH", .fn_enabled = enabled, .fn_hasparam = cmd_AUTH },
  { .name = 0 }
};

static const response* init(void)
{
  require_tls = getenv("AUTH_REQUIRES_TLS") != 0;

  if (!sasl_auth_init(&saslauth))
    return &resp_authfail;

  if (sasl_mechanisms != 0) {
    switch (sasl_auth_caps(&auth_caps)) {
    case 0: break;
    case 1: break;
    default:
      return &resp_authfail;
    }
  }

  return 0;
}

static const response* helo(str* hostname, str* capabilities)
{
  if (enabled())
    if (!str_cat(capabilities, &auth_caps)
	|| !str_catc(capabilities, '\n'))
      return &resp_oom;
  return 0;
  (void)hostname;
}

struct plugin plugin = {
  .version = PLUGIN_VERSION,
  .flags = 0,
  .commands = commands,
  .init = init,
  .helo = helo,
};
