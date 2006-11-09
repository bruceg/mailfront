#include <sysdeps.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <msg/msg.h>
#include "mailfront.h"

static response resp = { 250, 0 };
static str tmp;

static const response* sender(str* s)
{
  str_copys(&tmp, "Sender='");
  str_cat(&tmp, s);
  str_cats(&tmp, "'.");
  resp.message = tmp.s;
  return &resp;
}

static const response* recipient(str* r)
{
  str_copys(&tmp, "Recipient='");
  str_cat(&tmp, r);
  str_cats(&tmp, "'.");
  resp.message = tmp.s;
  return &resp;
}

static const response* message_end(int fd)
{
  struct stat st;
  char buf[1024];
  long rd;
  char* lf;
  char* ptr;

  /* Log the first two lines of the message, usually a Received: header */
  lseek(fd, 0, SEEK_SET);
  rd = read(fd, buf, sizeof buf - 1);
  buf[rd] = 0;
  if ((lf = strchr(buf, LF)) != 0) {
    str_copyb(&tmp, buf, lf-buf);
    ptr = lf + 1;
    if ((lf = strchr(ptr, LF)) != 0)
      str_catb(&tmp, ptr, lf-ptr);
    msg1(tmp.s);
  }

  fstat(fd, &st);
  str_copys(&tmp, "Received ");
  str_catu(&tmp, st.st_size);
  str_cats(&tmp, " bytes.");
  resp.message = tmp.s;
  return &resp;
  (void)fd;
}

struct plugin backend = {
  .sender = sender,
  .recipient = recipient,
  .message_end = message_end,
};
