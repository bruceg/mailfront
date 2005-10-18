#include <string.h>
#include <msg/msg.h>
#include "mailfront.h"

const response* backend_validate_init(void)
{
  return 0;
}

const response* backend_validate_sender(str* sender)
{
  return 0;
  (void)sender;
}

const response* backend_validate_recipient(str* recipient)
{
  return 0;
  (void)recipient;
}
