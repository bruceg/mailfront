#include "mailfront.h"

static const response* init(void)
{
  queuedir_init("QUEUEDIR");
  return 0;
}

struct plugin backend = {
  .version = PLUGIN_VERSION,
  .init = init,
  .reset = queuedir_reset,
  .sender = queuedir_sender,
  .recipient = queuedir_recipient,
  .data_start = queuedir_data_start,
  .data_block = queuedir_data_block,
  .message_end = queuedir_message_end,
};
