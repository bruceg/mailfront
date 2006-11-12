#include <sysdeps.h>
#include <sys/stat.h>
#include <unistd.h>
#include "mailfront.h"

static RESPONSE(no_file,451,"4.3.0 No temporary file was created");
static RESPONSE(stat,451,"4.3.0 Could not fstat temporary file");
static RESPONSE(not_file,451,"4.3.0 Temporary file is not a regular file");

static const response* data_start(int fd)
{
  struct stat st;
  if (fd < 0)
    return &resp_no_file;
  if (fstat(fd, &st) != 0)
    return &resp_stat;
  if (!S_ISREG(st.st_mode))
    return &resp_not_file;
  return 0;
}

struct plugin plugin = {
  .version = PLUGIN_VERSION,
  .flags = FLAG_NEED_FILE,
  .data_start = data_start,
};
