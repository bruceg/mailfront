#include "mailfront.h"
#include "smtp.h"

const char program[] = "smtpfront-echo";

extern int qmqp_mainloop(void);

int main(void)
{
  return qmqp_mainloop();
}
