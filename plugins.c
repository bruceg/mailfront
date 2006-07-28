#include <dlfcn.h>
#include <stdlib.h>
#include <string.h>
#include <msg/msg.h>
#include "mailfront.h"
#include "conf_modules.c"

static response resp_load = { 451, 0 };

struct plugin* plugin_list = 0;
struct plugin* plugin_tail = 0;

struct plugin* backend = 0;
struct protocol* protocol = 0;

void add_plugin(struct plugin* plugin)
{
  if (plugin_tail == 0)
    plugin_list = plugin;
  else
    plugin_tail->next = plugin;
  plugin_tail = plugin;
}

static void* load_object(const char* path,
			 const char* type,
			 const char* name)
{
  static str tmpstr;
  void* handle;
  void* ptr;
  str_copyf(&tmpstr, "s{/}s{-}s{.so}", path, type, name);
  if ((handle = dlopen(tmpstr.s, RTLD_NOW | RTLD_LOCAL)) == 0
      || (ptr = dlsym(handle, type)) == 0) {
    str_copyf(&tmpstr, "{4.3.0 Error loading }s{ }s{: }s",
	      type, name, dlerror());
    resp_load.message = tmpstr.s;
    return 0;
  }
  return ptr;
}

static const response* load_plugin(const char* path, const char* name)
{
  struct plugin* plugin;
  if ((plugin = load_object(path, "plugin", name)) == 0)
    return &resp_load;
  add_plugin(plugin);
  return 0;
}

static const response* load_plugins(const char* path)
{
  const char* list;
  const char* start;
  const char* end;
  long len;
  const response* resp;
  if ((list = getenv("PLUGINS")) == 0)
    list = "";
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

const response* load_modules(const char* protocol_name,
			     const char* backend_name)
{
  const char* path;
  if ((path = getenv("MODULE_PATH")) == 0)
    path = conf_modules;
  if ((protocol = load_object(path, "protocol", protocol_name)) == 0)
    return &resp_load;
  if ((backend = load_object(path, "backend", backend_name)) == 0)
    return &resp_load;
  return load_plugins(path);
}
