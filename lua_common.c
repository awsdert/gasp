#include "gasp.h"
int lua_panic_cb( lua_State *L ) {
	puts("Error: Lua traceback...");
	luaL_traceback(L,L,"[error]",-1);
	puts(lua_tostring(L,-1));
	return 0;
}

void lua_error_cb( lua_State *L, char const *text ) {
	fprintf(stderr,"%s\n", text);
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
	lua_pop(L,-1);
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
	lua_pop(L,-1);
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
	lua_pop(L,-1);
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
	lua_pop(L,-1);
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
	errno = EXIT_SUCCESS;
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

int lua_int2bytes( lua_State *L ) {
	lua_Integer val = luaL_checkinteger(L,1);
	size_t size = luaL_checkinteger(L,2), i = 0;
	char *v = (char*)&val;
	if ( size > sizeof(lua_Integer) )
		size = sizeof(lua_Integer);
	lua_createtable(L,size,0);
	for ( i = 0; i < size; ++i ) {
		lua_pushinteger(L,i+1);
		lua_pushinteger(L,v[i]);
		lua_settable(L,-3);
	}
	return 1;
}

int lua_tointbytes( lua_State *L ) {
	char const *text = luaL_checkstring(L,1);
	size_t leng = strlen(text), size = 0, i;
	int value = 0;
	
	lua_newtable(L);
	for ( i = 0; i < leng; ++i ) {
		if ( text[i] == ' ' ) {
			lua_pushinteger(L,++size);
			lua_pushinteger(L,value);
			lua_settable(L,-3);
			value = 0;
			continue;
		}
		
		value <<= CHAR_BIT;
		if ( value > UCHAR_MAX ) {
			lua_error_cb( L, "Invalid value" );
			return 1;
		}
		if ( text[i] >= '0' && text[i] <= '9' )
			value |= (text[i] - '0');
		else if ( text[i] >= 'A' && text[i] <= 'F' )
			value |= (text[i] - 'A');
		else if ( text[i] >= 'a' && text[i] <= 'a' )
			value |= (text[i] - 'a');
		else {
			lua_error_cb( L, "Invalid value" );
			return 1;
		}
	}
	lua_pushinteger(L,++size);
	lua_pushinteger(L,value);
	lua_settable(L,-3);
	push_branch_int( L, "__len", size );
	return 1;
}

int lua_totxtbytes( lua_State *L ) {
	size_t i, size;
	char *text;
	if ( !lua_istable(L,1) )
		return 0;
	lua_len(L,1);
	size = lua_tointeger(L,-1);
	text = calloc((size+1) * 3,1);
	lua_pushinteger( L, 1 );
	lua_gettable(L,1);
	sprintf( text, "%02X",
		(unsigned int)luaL_checkinteger(L,-1));
	for ( i = 1; i < size; ++i ) {
		lua_geti(L,1,i+1);
		sprintf( strchr(text,'\0'), " %02X",
			(unsigned int)luaL_checkinteger(L,-1));
		lua_pop(L,1);
	}
	lua_pushstring(L,text);
	free(text);
	return 1;
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
	lua_create_proc_classes(L);
	lua_newtable(L);
	push_branch_cfunc(L,"locate_app",lua_proc_locate_name);
	push_branch_cfunc(L,"new_glance",lua_proc_glance_grab);
	push_branch_cfunc(L,"new_handle",lua_proc_handle_grab);
	push_branch_cfunc(L,"get_endian",lua_get_endian);
	push_branch_cfunc(L,"totxtbytes",lua_totxtbytes);
	push_branch_cfunc(L,"tointbytes",lua_tointbytes);
	push_branch_cfunc(L,"int2bytes",lua_int2bytes);
	for ( i = 0; lua_path_funcs[i].name; ++i ) {
		reg = lua_path_funcs + i;
		push_branch_cfunc(L,reg->name,reg->func);
	}
	lua_setglobal(L,"gasp");
}
