#include "mailfront.h"

struct plugin backend = {
  .version = PLUGIN_VERSION,
  .reset = queuedir_reset,
  .sender = queuedir_sender,
  .recipient = queuedir_recipient,
  .data_start = queuedir_data_start,
  .data_block = queuedir_data_block,
  .message_end = queuedir_message_end,
};
