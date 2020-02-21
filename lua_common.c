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

void* lua_extract_bytes(
	int *err, lua_State *L, int index, nodes_t *dst ) {
	char const *text;
	char unexpected[64] = {0};
	uintmax_t ival;
	lua_Number	fval;
	uchar *array;
	node_t i, leng;
	size_t value = 0;
	if ( !(array = more_nodes( uchar, err, dst, BUFSIZ )) )
		return NULL;
	if ( lua_isstring(L,index) > 0 ) {
		if ( err )
			ERRMSG( EXIT_SUCCESS, "String" );
		text = lua_tostring(L,index);
		leng = strlen(text);
		if ( !leng || !(array = more_nodes( uchar, err, dst, leng )) )
			goto fail;
		for ( i = 0; i < leng; ++i ) {
			if ( text[i] == ' ' )
				continue;
			value <<= 8;
			if ( value > UCHAR_MAX ) {
				array[dst->count] = value >> 8;
				dst->count++;
				value = 0;
			}
			if ( text[i] >= '0' && text[i] <= '9' )
				value |= (text[i] - '0');
			else if ( text[i] >= 'A' && text[i] <= 'F' )
				value |= (text[i] - 'A') + 10;
			else if ( text[i] >= 'a' && text[i] <= 'f' )
				value |= (text[i] - 'a') + 10;
			else {
				sprintf( unexpected,
					"error('Invalid character in argument #%d"
					"at position %lu')", index, (ulong)i );
				luaL_dostring(L,unexpected);
				goto fail;
			}
		}
		array[dst->count] = value;
		dst->count++;
		return array;
	}
	else if ( lua_isinteger(L,index) ) {
		ival = lua_tointeger(L,index);
		for ( i = 0; i < sizeof(lua_Integer); ++i )
			array[i] = (ival >> (i * CHAR_BIT)) & UCHAR_MAX;
		dst->count = i;
		return array;
	}
	else if ( lua_isnumber(L,index) ) {
		fval = lua_tonumber(L,index);
		dst->count = sizeof(lua_Number);
		memcpy( array, &fval, sizeof(lua_Number) );
		return array;
	}
	else if ( lua_istable(L,index) ) {
		ival = luaL_checkinteger(L,index-1);
		for ( i = 0; i < ival; ++i ) {
			lua_geti(L,index,i+1);
			array[i] = lua_isnil(L,-1) ? 0 : luaL_checkinteger(L,-1);
			lua_pop(L,1);
		}
		dst->count = i;
		return array;
	}
	else if ( lua_isuserdata(L,index ) ) {
		ERRMSG( EXIT_SUCCESS, "Unexpected userdata value" );
		fprintf( stderr, "In argument #%d\n", index );
		goto fail;
	}
	else {
		dst->count = 1;
		array[0] = lua_isnil(L,index) ? 0 : lua_toboolean(L,index);
		return array;
	}
	fail:
	if ( err )
		ERRMSG( EXIT_SUCCESS, "Fail" );
	return NULL;
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

int lua_int2bytes( lua_State *L ) {
	node_t i, limit = luaL_optinteger(L,2,sizeof(lua_Integer));
	nodes_t nodes = {0};
	uchar *array;
	luaL_checkinteger(L,1);
	if ( !(array = lua_extract_bytes( NULL, L, 1, &nodes )) ) {
		free_nodes( uchar, NULL, &nodes );
		lua_newtable(L);
		lua_pushinteger( L, 0 );
		return 1;
	}
	lua_createtable(L,limit,1);
	push_branch_int( L, "__len", limit );
	for ( i = 0; i < limit; ++i ) {
		lua_pushinteger(L,i+1);
		lua_pushinteger(L,array[i]);
		lua_settable(L,-3);
	}
	lua_pushinteger( L, limit );
	free_nodes( uchar, NULL, &nodes );
	return 1;
}

int lua_tointbytes( lua_State *L ) {
	node_t i;
	nodes_t nodes = {0};
	uchar *array;
	if ( !(array = lua_extract_bytes(
		NULL, L, lua_istable(L,1) ? 2 : 1, &nodes ))
	) 
	{
		free_nodes( uchar, NULL, &nodes );
		lua_newtable(L);
		lua_pushinteger( L, 0 );
		return 2;
	}
	lua_createtable(L,nodes.count,1);
	for ( i = 0; i < nodes.count; ++i ) {
		lua_pushinteger(L,i+1);
		lua_pushinteger(L,array[i]);
		lua_settable(L,-3);
	}
	lua_pushinteger( L, nodes.count );
	free_nodes( uchar, NULL, &nodes );
	return 2;
}

int lua_flipbytes( lua_State *L ) {
	nodes_t nodes = {0};
	node_t i, j;
	uchar * array;
	if ( !(array = lua_extract_bytes(
		NULL, L, lua_istable(L,2) ? 2 : 1, &nodes )) ) {
		free_nodes( uchar, NULL, &nodes );
		lua_newtable(L);
		lua_pushinteger( L, 0 );
		return 2;
	}
	lua_createtable(L,nodes.count,1);
	for ( i = 0, j = nodes.count - 1; i < nodes.count; ++i, --j ) {
		lua_pushinteger(L,i+1);
		lua_pushinteger(L,array[j]);
		lua_settable(L,-3);
	}
	lua_pushinteger( L, nodes.count );
	free_nodes( uchar, NULL, &nodes );
	return 2;
}

int lua_totxtbytes( lua_State *L ) {
	node_t i;
	nodes_t nodes = {0}, text = {0};
	uchar *array;
	char *txt;
	if ( !(array = lua_extract_bytes(
		NULL, L, lua_istable(L,2) ? 2 : 1, &nodes ))
		|| !(txt = more_nodes(
		char, NULL, &text, nodes.count * CHAR_BIT )) ) {
		free_nodes( uchar, NULL, &nodes );
		lua_pushstring(L,"");
		return 1;
	}
	sprintf( txt, "%02X", *array );
	for ( i = 1; i < nodes.count; ++i )
		sprintf( strchr(txt,'\0'), " %02X", array[i] );
	lua_pushstring(L,txt);
	free_nodes( uchar, NULL, &nodes );
	free_nodes( uchar, NULL, &text );
	return 1;
}

int lua_bytes2int( lua_State *L ) {
	node_t i;
	nodes_t nodes = {0};
	uchar *array;
	uintmax_t val = 0;
	if ( !(array = lua_extract_bytes(
		NULL, L, lua_istable(L,2) ? 2 : 1, &nodes )) ) {
		free_nodes( uchar, NULL, &nodes );
		lua_pushinteger(L,0);
		return 1;
	}
	for ( i = 0; i < nodes.count; ++i ) {
		val <<= CHAR_BIT;
		val |= array[i];
	}
	lua_pushinteger(L,val);
	free_nodes( uchar, NULL, &nodes );
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
	lua_create_proc_classes(L);
	lua_newtable(L);
	push_branch_cfunc(L,"locate_app",lua_proc_locate_name);
	push_branch_cfunc(L,"new_glance",lua_proc_glance_grab);
	push_branch_cfunc(L,"new_handle",lua_proc_handle_grab);
	push_branch_cfunc(L,"get_endian",lua_get_endian);
	push_branch_cfunc(L,"totxtbytes",lua_totxtbytes);
	push_branch_cfunc(L,"tointbytes",lua_tointbytes);
	push_branch_cfunc(L,"int2bytes",lua_int2bytes);
	push_branch_cfunc(L,"str2bytes",lua_str2bytes);
	push_branch_cfunc(L,"bytes2int",lua_bytes2int);
	push_branch_cfunc(L,"flipbytes",lua_flipbytes);
	push_branch_cfunc(L,"set_reboot_gui",lua_set_reboot_gui);
	push_branch_cfunc(L,"get_reboot_gui",lua_get_reboot_gui);
	push_branch_cfunc(L,"toggle_reboot_gui",lua_toggle_reboot_gui);
	for ( i = 0; lua_path_funcs[i].name; ++i ) {
		reg = lua_path_funcs + i;
		push_branch_cfunc(L,reg->name,reg->func);
	}
	lua_setglobal(L,"gasp");
}
