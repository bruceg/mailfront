#include "mailfront.h"

static const response* init(void)
{
  return 0;
}

static const response* helo(str* hostname)
{
  return 0;
}

static const response* reset(void)
{
  return 0;
}

static const response* sender(str* sender)
{
  return 0;
}

static const response* recipient(str* recipient)
{
  return 0;
}

static const response* data_start(int fd)
{
  return 0;
}

static const response* data_block(const char* bytes, unsigned long len)
{
}

static const response* message_end(int fd)
{
  return 0;
}

struct plugin plugin = {
  .flags = 0,
  .init = init,
  .helo = helo,
  .reset = reset,
  .sender = sender,
  .recipient = recipient,
  .data_start = data_start,
  .data_block = data_block,
  .message_end = message_end,
};
