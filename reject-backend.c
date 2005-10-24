#include <stdlib.h>
#include <unistd.h>
#include "mailfront.h"
#include "responses.h"

static response resp = {451,"You are not allowed to use this mail server."};

int authenticated = 0;
const int authenticating = 0;
unsigned long maxdatabytes = 0;
const char UNKNOWN[] = "unknown";

int number_ok(const response* r)
{
  return r->number < 400;
}

const response* handle_init(void)
{
  return 0;
}

const response* handle_reset(void)
{
  return 0;
}

const response* handle_sender(str* unused)
{
  return &resp;
  (void)unused;
}

const response* handle_recipient(str* unused)
{
  return &resp;
  (void)unused;
}

const response* handle_data_start(const char* unused1, const char* unused2)
{
  return &resp;
  (void)unused1;
  (void)unused2;
}

void handle_data_bytes(const char* unused1, unsigned unused2)
{
  (void)unused1;
  (void)unused2;
}

const response* handle_data_end(void)
{
  return &resp;
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
