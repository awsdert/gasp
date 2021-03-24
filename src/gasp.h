#ifndef GASP_H
#define GASP_H

#include <workers.h>
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ptrace.h>
#include <sys/wait.h>
#include <sys/uio.h>

#include <dirent.h>

typedef unsigned char uchar;

int lua_dumpstack( lua_State *L );
int lua_panic_cb( lua_State *L );
void push_global_cfunc( lua_State *L, char const *key, lua_CFunction val );
void push_global_bool( lua_State *L, char const *key, bool val );
void push_global_int( lua_State *L, char const *key, int val );
void push_global_num( lua_State *L, char const *key, long double val );
void push_global_obj( lua_State *L, char const *key );
void push_global_str( lua_State *L, char const *key, char *val );
void push_branch_cfunc( lua_State *L, char const *key, lua_CFunction val );
void push_branch_bool( lua_State *L, char const *key, bool val );
void push_branch_int( lua_State *L, char const *key, int val );
void push_branch_num( lua_State *L, char const *key, long double val );
void push_branch_obj( lua_State *L, char const *key );
void push_branch_str( lua_State *L, char const *key, char *val );
void lua_create_gasp(lua_State *L);


#endif
