#include "mailfront.h"
#include "starttls.h"

static RESPONSE(start, 220, "2.0.0 Ready to start TLS");

static const response* helo(str* hostname, str* capabilities)
{
  if (starttls_available())
    if (!str_cats(capabilities, "STARTTLS\n"))
      return &resp_oom;
  return NULL;
  (void)hostname;
}

static int cmd_STARTTLS(void)
{
  if (!respond(&resp_start))
    return 0;

  if (!starttls_start())
    return 0;

  starttls_disable();
  session_setnum("tls_state", 1);
  return 1;
}

static const struct command commands[] = {
  { "STARTTLS", .fn_enabled = starttls_available, .fn_noparam = cmd_STARTTLS },
  { .name = 0 }
};

struct plugin plugin = {
  .version = PLUGIN_VERSION,
  .commands = commands,
  .init = starttls_init,
  .helo = helo,
};
