#include <stdlib.h>
#include <unistd.h>
#include "mailfront.h"
#include "responses.h"

static response resp = {451,"You are not allowed to use this mail server."};

const int authenticating = 0;
unsigned long maxdatabytes = 0;
const char UNKNOWN[] = "unknown";

const response* handle_init(struct session* unused)
{
  return 0;
  (void)unused;
}

const response* handle_reset(struct session* session)
{
  session->relayclient = "";
  return 0;
}

const response* handle_sender(struct session* unused1, str* unused2)
{
  return &resp;
  (void)unused1;
  (void)unused2;
}

const response* handle_recipient(struct session* unused1, str* unused2)
{
  return &resp;
  (void)unused1;
  (void)unused2;
}

const response* handle_data_start(struct session* unused)
{
  return &resp;
  (void)unused;
}

void handle_data_bytes(struct session* unused1,
		       const char* unused2, unsigned unused3)
{
  (void)unused1;
  (void)unused2;
  (void)unused3;
}

const response* handle_data_end(struct session* unused)
{
  return &resp;
  (void)unused;
}

extern int mainloop(void);

int main(int argc, char* argv[])
{
  const char* sr;
  if ((sr = getenv("SMTPREJECT")) != 0) {
    if (sr[0] == '-') {
      ++sr;
      resp.number = 553;
    }
    resp.message = sr;
    return mainloop();
  }
  else {
    execvp(argv[1], argv+1);
    return 1;
  }
  (void)argc;
}
