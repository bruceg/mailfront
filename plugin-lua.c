#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
#include <msg/msg.h>
#include "mailfront.h"

static int l_msg(lua_State *L)
{
  int i;
  for (i = 1; i <= lua_gettop(L); i++)
    msg1(lua_tostring(L, i));
  return 0;
}

static int l_getenv(lua_State *L)
{
  int i;
  const int nparams = lua_gettop(L);
  for (i = 1; i < nparams; i++) {
    const char* name = lua_tostring(L, i);
    const char* value = session_getenv(name);
    if (value == 0)
      lua_pushnil(L);
    else
      lua_pushstring(L, value);
  }
  return nparams;
}

static int l_putenv(lua_State *L)
{
  const int nparams = lua_gettop(L);
  int i;
  for (i = 1; i < nparams; i++) {
    if (!session_putenv(lua_tostring(L, i))) {
      lua_pushstring(L, "Out of memory");
      lua_error(L);
    }
  }
  return 0;
}

static int l_setenv(lua_State *L)
{
  if (lua_gettop(L) != 3) {
    lua_pushstring(L, "Incorrect number of parameters to setenv");
    lua_error(L);
  }
  const char* name = lua_tostring(L, 1);
  const char* value = lua_tostring(L, 2);
  int overwrite = lua_tointeger(L, 3);
  if (!session_setenv(name, value, overwrite)) {
    lua_pushstring(L, "setenv failed");
    lua_error(L);
  }
  return 0;
}

static int l_delnum(lua_State *L)
{
  const int nparams = lua_gettop(L);
  int i;
  for (i = 1; i < nparams; i++)
    session_delnum(lua_tostring(L, i));
  return 0;
}

static int l_delstr(lua_State *L)
{
  const int nparams = lua_gettop(L);
  int i;
  for (i = 1; i < nparams; i++)
    session_delstr(lua_tostring(L, i));
  return 0;
}

static int l_getnum(lua_State *L)
{
  if (lua_gettop(L) != 2) {
    lua_pushstring(L, "Incorrect number of parameters to getnum");
    lua_error(L);
  }
  const char* name = lua_tostring(L, 1);
  unsigned long dflt = lua_tonumber(L, 2);
  lua_pushnumber(L, session_getnum(name, dflt));
  return 1;
}

static int l_getstr(lua_State *L)
{
  const int nparams = lua_gettop(L);
  int i;
  for (i = 1; i < nparams; i++) {
    const char* s = session_getstr(lua_tostring(L, i));
    if (s != 0)
      lua_pushstring(L, s);
    else
      lua_pushnil(L);
  }
  return nparams;
}

static int l_setnum(lua_State *L)
{
  if (lua_gettop(L) != 2) {
    lua_pushstring(L, "Incorrect number of parameters to setnum");
    lua_error(L);
  }
  const char* name = lua_tostring(L, 1);
  unsigned long value = lua_tonumber(L, 2);
  session_setnum(name, value);
  return 0;
}

static int l_setstr(lua_State *L)
{
  if (lua_gettop(L) != 2) {
    lua_pushstring(L, "Incorrect number of parameters to setstr");
    lua_error(L);
  }
  const char* name = lua_tostring(L, 1);
  const char* value = lua_tostring(L, 2);
  session_setstr(name, value);
  return 0;
}

static const luaL_Reg library[] = {
  { "msg", l_msg },

  { "getenv", l_getenv },
  { "putenv", l_putenv },
  { "setenv", l_setenv },

  { "delnum", l_delnum },
  { "delstr", l_delstr },
  { "getnum", l_getnum },
  { "getstr", l_getstr },
  { "setnum", l_setnum },
  { "setstr", l_setstr },

  { NULL, NULL }
};

static lua_State* L = 0;

static const response* get_response(void)
{
  static response resp;
  static str msgcopy;
  const char* msg;

  resp.number = lua_tonumber(L, -2);
  msg = lua_tostring(L, -1);
  if (msg == 0 || *msg == 0) {
    resp.message = (resp.number < 400) ? "OK"
      : (resp.number < 500) ? "Deferred"
      : "Rejected";
  }
  else {
    if (!str_copys(&msgcopy, msg))
      return &resp_oom;
    resp.message = msgcopy.s;
  }
  lua_pop(L, 2);
  return (resp.number > 0) ? &resp : 0;
}

static int setup(const char* name)
{
  if (L != 0) {
    lua_getfield(L, LUA_GLOBALSINDEX, name);
    if (lua_isfunction(L, -1))
      return 1;
    lua_pop(L, 1);
  }
  return 0;
}

static const response* callit(int nparams)
{
  int code;
  if ((code = lua_pcall(L, nparams, 2, 0)) != 0) {
    msgf("{Lua error: }d{ (}s{)}", code, lua_tostring(L, -1));
    lua_pop(L, 1);
    return &resp_internal;
  }
  return get_response();
}

static const response* init(void)
{
  const char* env;
  const luaL_Reg *lp;

  env = session_getenv("LUA_SCRIPT");
  if (env != 0) {
    L = luaL_newstate();
    if (L == 0)
      return &resp_oom;
    switch (luaL_loadfile(L, env)) {
    case LUA_ERRMEM:
      return &resp_oom;
    case 0:
      break;
    case LUA_ERRFILE:
      msg3("Cannot read \"", env, "\"");
      return &resp_internal;
    case LUA_ERRSYNTAX:
      msg3("Syntax error in \"", env, "\"");
      return &resp_internal;
    default:
      return &resp_internal;
    }
    for (lp = library; lp->name != 0; lp++)
      lua_register(L, lp->name, lp->func);
    return callit(0);
  }
  return 0;
}

static const response* reset(void)
{
  if (setup("reset"))
    return callit(0);
  return 0;
}

static const response* helo(str* hostname, str* capabilities)
{
  if (setup("helo")) {
    lua_pushlstring(L, hostname->s, hostname->len);
    lua_pushlstring(L, capabilities->s, capabilities->len);
    return callit(2);
  }
  return 0;
}

static const response* sender(str* address, str* params)
{
  if (setup("sender")) {
    lua_pushlstring(L, address->s, address->len);
    lua_pushlstring(L, params->s, params->len);
    return callit(2);
  }
  return 0;
  (void)params;
}

static const response* recipient(str* address, str* params)
{
  if (setup("recipient")) {
    lua_pushlstring(L, address->s, address->len);
    lua_pushlstring(L, params->s, params->len);
    return callit(2);
  }
  return 0;
  (void)params;
}

static const response* data_start(int fd)
{
  if (setup("data_start")) {
    lua_pushinteger(L, fd);
    return callit(1);
  }
  return 0;
}

static const response* data_block(const char* bytes, unsigned long len)
{
  if (setup("data_block")) {
    lua_pushlstring(L, bytes, len);
    return callit(1);
  }
  return 0;
}

static const response* message_end(int fd)
{
  if (setup("message_end")) {
    lua_pushinteger(L, fd);
    return callit(1);
  }
  return 0;
}

struct plugin plugin = {
  .version = PLUGIN_VERSION,
  .flags = 0,
  .init = init,
  .reset = reset,
  .helo = helo,
  .sender = sender,
  .recipient = recipient,
  .data_start = data_start,
  .data_block = data_block,
  .message_end = message_end,
};
