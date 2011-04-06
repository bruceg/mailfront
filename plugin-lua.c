#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
#include "mailfront.h"
#include <msg/msg.h>

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

static const response* helo(str* hostname)
{
  if (setup("helo")) {
    lua_pushlstring(L, hostname->s, hostname->len);
    return callit(1);
  }
  return 0;
}

static const response* sender(str* address)
{
  if (setup("sender")) {
    lua_pushlstring(L, address->s, address->len);
    return callit(1);
  }
  return 0;
}

static const response* recipient(str* address)
{
  if (setup("recipient")) {
    lua_pushlstring(L, address->s, address->len);
    return callit(1);
  }
  return 0;
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
