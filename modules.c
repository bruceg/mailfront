#include <dlfcn.h>
#include <stdlib.h>
#include <string.h>
#include <msg/msg.h>
#include "mailfront-internal.h"
#include "conf_modules.c"

static response resp_load = { 451, 0 };
static RESPONSE(backend_version,451,"4.3.0 Backend ABI version mismatch");
static RESPONSE(plugin_version,451,"4.3.0 Plugin ABI version mismatch");
static RESPONSE(protocol_version,451,"4.3.0 Protocol ABI version mismatch");

static void append_plugin(struct plugin* plugin)
{
  plugin->next = 0;
  if (session.plugin_tail == 0)
    session.plugin_list = plugin;
  else
    session.plugin_tail->next = plugin;
  session.plugin_tail = plugin;
}

static void prepend_plugin(struct plugin* plugin)
{
  plugin->next = session.plugin_list;
  session.plugin_list = plugin;
  if (session.plugin_tail == 0)
    session.plugin_tail = plugin;
}

static struct plugin* remove_plugin(const char* name)
{
  struct plugin* curr;
  struct plugin* prev;
  if (name[0] == '*' && name[1] == 0)
    session.plugin_list = session.plugin_tail = 0;
  else {
    for (prev = 0, curr = session.plugin_list;
	 curr != 0;
	 prev = curr, curr = curr->next) {
      if (strcmp(curr->name, name) == 0) {
	if (((prev == 0)
	     ? (session.plugin_list = curr->next)
	     : (prev->next = curr->next)) == 0)
	  session.plugin_tail = prev;
	return curr;
      }
    }
  }
  return 0;
}

static void* load_object(const char* type,
			 const char* name)
{
  static str tmpstr;
  void* handle;
  void* ptr;
  str_copyf(&tmpstr, "s{/}s{-}s{.so}", session.module_path, type, name);
  if ((handle = dlopen(tmpstr.s, RTLD_NOW | RTLD_LOCAL)) == 0
      || (ptr = dlsym(handle, type)) == 0) {
    str_copyf(&tmpstr, "{4.3.0 Error loading }s{ }s{: }s",
	      type, name, dlerror());
    resp_load.message = tmpstr.s;
    return 0;
  }
  return ptr;
}

static struct plugin* find_builtin_plugin(const char* name)
{
  int i;
  for (i = 0; builtin_plugins[i].name != 0; ++i) {
    if (strcmp(builtin_plugins[i].name, name) == 0)
      return &builtin_plugins[i];
  }
  return 0;
}

static const response* load_plugin(const char* name)
{
  struct plugin* plugin;
  void (*add)(struct plugin*);
  if (name[0] == '-') {
    remove_plugin(++name);
    return 0;
  }
  if (name[0] == '+') {
    ++name;
    add = prepend_plugin;
  }
  else
    add = append_plugin;
  if ((plugin = remove_plugin(name)) == 0) {
    if ((plugin = find_builtin_plugin(name)) == 0) {
      if ((plugin = load_object("plugin", name)) == 0)
	return &resp_load;
      if (plugin->version != PLUGIN_VERSION)
	return &resp_plugin_version;
      if ((plugin->name = strdup(name)) == 0)
	return &resp_oom;
    }
  }
  add(plugin);
  session.flags |= plugin->flags;
  return 0;
}

static const response* load_plugins(const char* list)
{
  const char* start;
  const char* end;
  long len;
  const response* resp;
  for (start = list; *start != 0; start = end) {
    end = start;
    while (*end != 0 && *end != ':')
      ++end;
    len = end - start;
    if (len > 0) {
      char copy[len+1];
      memcpy(copy, start, len);
      copy[len] = 0;
      if ((resp = load_plugin(copy)) != 0)
	return resp;
    }
    if (*end == ':')
      ++end;
  }
  return 0;
}

const response* load_modules(const char* protocol_name,
			     const char* backend_name,
			     const char** plugins)
{
  const char* env;
  const response* r;
  if ((session.module_path = getenv("MODULE_PATH")) == 0)
    session.module_path = conf_modules;
  if ((session.protocol = load_object("protocol", protocol_name)) == 0)
    return &resp_load;
  if (session.protocol->version != PROTOCOL_VERSION)
    return &resp_protocol_version;
  if ((session.backend = load_object("backend", backend_name)) == 0)
    return &resp_load;
  if (session.backend->version != PLUGIN_VERSION)
    return &resp_backend_version;
  session.flags |= session.backend->flags;
  while (*plugins != 0)
    if ((r = load_plugins(*plugins++)) != 0)
      return r;
  if ((env = getenv("PLUGINS")) != 0)
    return load_plugins(env);
  return 0;
}
