#include <stdlib.h>
#include <unistd.h>
#include "mailfront.h"
#include "smtp.h"
#include "iobuf/iobuf.h"
#include "qmail.h"

const char program[] = "smtpfront-reject";

static response resp = {0,451,"You are not allowed to use this mail server."};

void handle_reset(void)
{
}

const response* handle_sender(str* unused)
{
  return &resp;
}

const response* handle_recipient(str* unused)
{
  return &resp;
}

const response* handle_data_start(void)
{
  return &resp;
}

void handle_data_bytes(const char* unused1, unsigned long unused2)
{
}

const response* handle_data_end(void)
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
    return smtp_mainloop(0);
  }
  else {
    execvp(argv[1], argv+1);
    return 1;
  }
}
