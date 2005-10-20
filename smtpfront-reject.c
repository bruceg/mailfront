#include <stdlib.h>
#include <unistd.h>
#include "mailfront.h"
#include "smtp.h"
#include <iobuf/iobuf.h>

const char program[] = "smtpfront-reject";

static response resp = {451,"You are not allowed to use this mail server."};

const response* backend_validate_init(void)
{
  return 0;
}

void backend_handle_reset(void)
{
  relayclient = 0;
  authenticated = 0;
}

const response* backend_validate_sender(str* unused)
{
  return &resp;
  (void)unused;
}

const response* backend_validate_recipient(str* unused)
{
  return &resp;
  (void)unused;
}

const response* backend_handle_sender(str* unused)
{
  return &resp;
  (void)unused;
}

const response* backend_handle_recipient(str* unused)
{
  return &resp;
  (void)unused;
}

const response* backend_handle_data_start(void)
{
  return &resp;
}

void backend_handle_data_bytes(const char* unused1, unsigned long unused2)
{
  (void)unused1;
  (void)unused2;
}

const response* backend_handle_data_end(void)
{
  return &resp;
}

int main(int argc, char* argv[])
{
  const char* sr;
  if ((sr = getenv("SMTPREJECT")) != 0) {
    if (sr[0] == '-') {
      ++sr;
      resp.number = 553;
    }
    resp.message = sr;
    return smtp_mainloop();
  }
  else {
    execvp(argv[1], argv+1);
    return 1;
  }
  (void)argc;
}
