/* Prototype plugin file.
 * Remove any functions that are not needed.
 */

#include "mailfront.h"

/* The init function is called once after the plugin is loaded. */
static const response* init(void)
{
  return 0;
}

/* The helo function is called once by the SMTP protocol when either the
 * HELO or EHLO command is issued.  The parameter is the hostname given
 * in the command. */
static const response* helo(str* hostname, str* capabilities)
{
  return 0;
}

/* The reset function is called at least once per message before any of
 * the envelope or data is processed. */
static const response* reset(void)
{
  return 0;
}

/* The sender function is called exactly once per message. The parameter
 * is the sender email addres, and may be modified. */
static const response* sender(str* address, str* params)
{
  return 0;
}

/* The recipient function is called one or more times per message, once
 * for each recipient.  The parameter is the recipient email address,
 * and may be modified. */
static const response* recipient(str* address, str* params)
{
  return 0;
}

/* The data_start function is called when the sender starts transmitting
 * data.  Depending on the protocol, this may happen before (QMTP) or
 * after (SMTP) the envelope is processed.  The parameter is the file
 * descriptor of the temporary file, or -1 if none was created. */
static const response* data_start(int fd)
{
  return 0;
}

/* The data_block function is called once for each block of data
 * received by the protocol.  If this function returns an error
 * response, further data blocks will be received but not processed. */
static const response* data_block(const char* bytes, unsigned long len)
{
}

/* The message_end function is called after both the message envelope
 * and data have been completely transmitted.  The parameter is the file
 * descriptor of the temporary file, or -1 if none was created. */
static const response* message_end(int fd)
{
  return 0;
}

/* To define a new command, write the function that will be executed
 * when the command is invoked, and add it to a "struct command" array
 * as below. The function is passed the entire command argument, and
 * must handle sending responses itself. Return positive for success or
 * 0 to disconnect. */
static int cmd_X_CMD0(void)
{
  return 1;
}

static int cmd_X_CMD1(str* param)
{
  return 1;
}

static const struct command commands[] = {
  { "X-CMD0", .fn_noparam = cmd_X_CMD0 },
  { "X-CMD1", .fn_hasparam = cmd_X_CMD1 },
  { 0, 0 }
};

/* Plugins must export this structure.
 * Remove any of the entries that are not used (ie 0 or NULL).
 */
struct plugin plugin = {
  .version = PLUGIN_VERSION,
  .flags = 0,
  .commands = commands,
  .init = init,
  .helo = helo,
  .reset = reset,
  .sender = sender,
  .recipient = recipient,
  .data_start = data_start,
  .data_block = data_block,
  .message_end = message_end,
};
