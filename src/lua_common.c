#include "gasp.h"

/* Robbed from
 * https://stackoverflow.com/questions/59091462/from-c-how-can-i-print-the-contents-of-the-lua-stack
 */

int lua_dumpstack(lua_State *L)
{
	int top=lua_gettop(L);
	printf("Printing %d stack elements\n", top);
	for (int i=1; i <= top; i++)
	{
		printf("%d\t%s\t", i, luaL_typename(L,i));
		switch (lua_type(L, i))
		{
			case LUA_TNUMBER:
				printf("%g\n",lua_tonumber(L,i));
				break;
			case LUA_TSTRING:
				printf("%s\n",lua_tostring(L,i));
				break;
			case LUA_TBOOLEAN:
				printf("%s\n", (lua_toboolean(L, i) ? "true" : "false"));
				break;
			case LUA_TNIL:
				printf("%s\n", "nil");
				break;
			default:
				printf("%p\n",lua_topointer(L,i));
				break;
		}
	}
	return 0;
}

int lua_panic_cb( lua_State *L ) {
	lua_dumpstack(L);
	luaL_traceback(L,L,NULL,0);
	printf( "Error, lua tracback:\n%s", lua_tostring(L,-1) );
	return 0;
}

void lua_error_cb( lua_State *L, char const *text ) {
	printf( "%s", text);
	lua_panic_cb(L);
}

void push_global_cfunc(
	lua_State *L, char const *key, lua_CFunction val
)
{
	lua_pushcfunction(L,val);
	lua_setglobal(L,key);
}

void push_global_str( lua_State *L, char const *key, char *val ) {
	lua_pushstring(L,val);
	lua_setglobal(L,key);
}

void push_global_int( lua_State *L, char const *key, int val ) {
	lua_pushinteger(L,val);
	lua_setglobal(L,key);
}

void push_global_num( lua_State *L, char const *key, long double val ) {
	lua_pushnumber(L,val);
	lua_setglobal(L,key);
}

void push_global_bool( lua_State *L, char const *key, bool val ) {
	lua_pushboolean(L,val);
	lua_setglobal(L,key);
}

void push_global_obj( lua_State *L, char const *key ) {
	lua_newtable(L);
	lua_setglobal(L,key);
}


void push_branch_cfunc(
	lua_State *L, char const *key, lua_CFunction val
)
{
	lua_pushstring(L,key);
	lua_pushcfunction(L,val);
	lua_settable(L,-3);
}

void push_branch_str( lua_State *L, char const *key, char *val ) {
	lua_pushstring(L,key);
	lua_pushstring(L,val);
	lua_settable(L,-3);
}

void push_branch_int( lua_State *L, char const *key, int val ) {
	lua_pushstring(L,key);
	lua_pushinteger(L,val);
	lua_settable(L,-3);
}

void push_branch_num( lua_State *L, char const *key, long double val ) {
	lua_pushstring(L,key);
	lua_pushnumber(L,val);
	lua_settable(L,-3);
}

void push_branch_bool( lua_State *L, char const *key, bool val ) {
	lua_pushstring(L,key);
	lua_pushboolean(L,val);
	lua_settable(L,-3);
}

void push_branch_obj( lua_State *L, char const *key ) {
	lua_pushstring(L,key);
	lua_newtable(L);
	lua_settable(L,-3);
}

bool find_branch_key( lua_State *L, char const *key ) {
	if ( !L || !key ) {
		lua_error_cb( L, "C function gave NULL parameter" );
		return 0;
	}
	lua_pushstring(L, key);
	lua_gettable(L, -2);
	return 1;
}

bool find_branch_str( lua_State *L, char const *key, char const **val ) {
	if ( !find_branch_key(L,key) || !val ) {
		lua_error_cb( L, "C function gave NULL parameter" );
		return 0;
	}
	*val = lua_isstring(L,-1) ? lua_tostring(L,-1) : NULL;
	lua_pop(L,1);
	return (*val != NULL);
}

bool find_branch_int( lua_State *L, char const *key, int *val ) {
	_Bool valid = 0;
	if ( !find_branch_key(L,key) || !val ) {
		lua_error_cb( L, "C function gave NULL parameter" );
		return 0;
	}
	*val = (valid = (lua_isinteger(L,-1) || lua_isnumber(L,-1)))
		? lua_tointeger(L,-1) : 0;
	lua_pop(L,1);
	return valid;
}

bool find_branch_num( lua_State *L, char const *key, long double *val ) {
	_Bool valid = 0;
	if ( !find_branch_key(L,key) || !val ) {
		lua_error_cb( L, "C function gave NULL parameter" );
		return 0;
	}
	*val = (valid = (lua_isnumber(L,-1) || lua_isinteger(L,-1)))
		? lua_tonumber(L,-1) : 0;
	lua_pop(L,1);
	return valid;
}

bool find_branch_bool( lua_State *L, char const *key, bool *val ) {
	_Bool valid = 0;
	if ( !find_branch_key(L,key) || !val ) {
		lua_error_cb( L, "C function gave NULL parameter" );
		return 0;
	}
	*val = (valid = (lua_isboolean(L,-1)))
		? lua_toboolean(L,-1) : 0;
	lua_pop(L,1);
	return valid;
}

/* Functions for lua global 'gasp' */

int lua_path_access( lua_State *L ) {
	char const *path = luaL_checkstring(L,1);
	int perm = luaL_optinteger(L,2,0);
	lua_pushinteger( L, access( path, perm ) );
	return 1;
}

int lua_path_exists( lua_State *L ) {
	char const *path = luaL_checkstring(L,1);
	struct stat info = {0};
	if ( stat( path, &info ) == 0 )
		lua_pushboolean( L, 1 );
	else
		lua_pushboolean( L, (errno != ENOTDIR && errno != ENOENT) );
	return 1;
}
int lua_path_isdir( lua_State *L ) {
	char const *path = luaL_checkstring(L,1);
	DIR *dir = opendir(path);
	errno = 0;
	if ( dir ) {
		lua_pushinteger(L,1);
		closedir(dir);
		return 1;
	}
	lua_pushinteger( L,
		(errno == ENOENT || errno == ENOTDIR) ? 0 : -1 );
	return 1;
}
int lua_path_isfile( lua_State *L ) {
	char const *path = luaL_checkstring(L,1);
	struct stat info = {0};
	if ( stat( path, &info ) == 0 )
		lua_pushinteger( L, ((info.st_mode & S_IFMT) == S_IFREG) );
	else lua_pushinteger( L,
		(errno == ENOENT || errno == ENOTDIR) ? 0 : -1 );
	return 1;
}
int lua_path_files( lua_State *L ) {
	char const *path = luaL_checkstring(L,1);
	DIR *dir = opendir(path);
	struct dirent *ent;
	size_t i = 0;
	lua_newtable(L);
	if ( !dir )
		return 1;
	while ( (ent = readdir(dir)) ) {
		if ( strcmp("..",ent->d_name) == 0
			|| strcmp(".",ent->d_name) == 0 )
			continue;
		lua_pushinteger(L,++i);
		lua_pushstring(L,ent->d_name);
		lua_settable(L,-3);
	}
	closedir(dir);
	return 1;
}

int lua_get_endian( lua_State *L ) {
	long val = 0x12345678;
	uchar *bytes = (uchar*)&val;
	switch ( *bytes ) {
	case 0x78: lua_pushstring(L, "Little"); return 1;
	case 0x12: lua_pushstring(L, "Big"); return 1;
	case 0x56: lua_pushstring(L, "LPDP"); return 1;
	case 0x34: lua_pushstring(L, "BPDP"); return 1;
	}
	switch ( bytes[4] ) {
	case 0x78: lua_pushstring(L, "Little"); return 1;
	case 0x12: lua_pushstring(L, "Big"); return 1;
	case 0x56: lua_pushstring(L, "LPDP"); return 1;
	case 0x34: lua_pushstring(L, "BPDP"); return 1;
	}
	return 0;
}

int lua_str2bytes( lua_State *L ) {
	char const *str = luaL_checkstring(L,1);
	size_t i, len = strlen(str);
	lua_createtable( L, len, 0 );
	for ( i = 0; i < len; ++i ) {
		lua_pushinteger( L, i + 1 );
		lua_pushinteger( L, str[i] );
		lua_settable( L, -3 );
	}
	lua_pushinteger( L, i + 1 );
	lua_pushinteger( L, '\0' );
	lua_settable( L, -3 );
	lua_pushinteger( L, len+1 );
	return 2;
}

int lua_ptrlimit( lua_State *L ) {
	uintmax_t limit = (uintmax_t)((void*)(~0));
	limit >>= 1;
	lua_pushinteger( L, limit );
	return 1;
}

bool g_reboot_gui = false;

int lua_get_reboot_gui( lua_State *L ) {
	lua_pushboolean(L,g_reboot_gui);
	return 1;
}

int lua_set_reboot_gui( lua_State *L ) {
	g_reboot_gui = lua_toboolean(L,1);
	return lua_get_reboot_gui(L);
}

int lua_toggle_reboot_gui( lua_State *L ) {
	g_reboot_gui = !g_reboot_gui;
	return lua_get_reboot_gui(L);
}

luaL_Reg lua_path_funcs[] = {
	{ "path_access", lua_path_access },
	{ "path_exists", lua_path_exists },
	{ "path_isdir", lua_path_isdir },
	{ "path_isfile", lua_path_isfile },
	{ "path_files",lua_path_files },
{NULL,NULL}};

void lua_create_gasp(lua_State *L) {
	size_t i;
	luaL_Reg *reg;
	lua_newtable(L);
	push_branch_cfunc(L,"ptrlimit",lua_ptrlimit );
	push_branch_cfunc(L,"get_endian",lua_get_endian);
	push_branch_cfunc(L,"set_reboot_gui",lua_set_reboot_gui);
	push_branch_cfunc(L,"get_reboot_gui",lua_get_reboot_gui);
	push_branch_cfunc(L,"toggle_reboot_gui",lua_toggle_reboot_gui);
	for ( i = 0; lua_path_funcs[i].name; ++i ) {
		reg = lua_path_funcs + i;
		push_branch_cfunc(L,reg->name,reg->func);
	}
	lua_setglobal(L,"gasp");
}
