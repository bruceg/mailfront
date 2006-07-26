#include <dlfcn.h>
#include <stdlib.h>
#include <string.h>
#include <msg/msg.h>
#include "mailfront.h"
#include "conf_modules.c"

static response resp_load = { 451, 0 };

struct plugin* plugin_list = 0;
struct plugin* plugin_tail = 0;

void add_plugin(struct plugin* plugin)
{
  if (plugin_tail == 0)
    plugin_list = plugin;
  else
    plugin_tail->next = plugin;
  plugin_tail = plugin;
}

static const response* load_plugin(const char* path, const char* name)
{
  static str tmpstr;
  void* handle;
  struct plugin* plugin;
  str_copyf(&tmpstr, "s{/plugin-}s{.so}", path, name);
  if ((handle = dlopen(tmpstr.s, RTLD_NOW | RTLD_LOCAL)) == 0
      || (plugin = dlsym(handle, "plugin")) == 0) {
    str_copyf(&tmpstr, "{4.3.0 Error loading plugin }s{: }s",
	      name, dlerror());
    resp_load.message = tmpstr.s;
    return &resp_load;
  }
  add_plugin(plugin);
  /* Don't even try to dlclose the plugin, since we need to keep the code. */
  return 0;
}

const response* load_plugins(void)
{
  const char* list;
  const char* path;
  const char* start;
  const char* end;
  long len;
  const response* resp;
  if ((path = getenv("PLUGINS_PATH")) == 0)
    path = conf_modules;
  if ((list = getenv("PLUGINS")) == 0)
    list = default_plugins;
  for (start = list; *start != 0; start = end) {
    end = start;
    while (*end != 0 && *end != ':')
      ++end;
    len = end - start;
    if (len > 0) {
      char copy[len+1];
      memcpy(copy, start, len);
      copy[len] = 0;
      if ((resp = load_plugin(path, copy)) != 0)
	return resp;
    }
    if (*end == ':')
      ++end;
  }
  return 0;
}
