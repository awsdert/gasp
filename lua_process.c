#include "gasp.h"
#define PROC_GLANCE_CLASS "class_proc_glance"

int lua_proc_notice_info( lua_State *L, proc_notice_t *notice )
{
	if ( !notice )
		return 0;
	lua_createtable( L, 0, 5 );
	push_branch_bool( L, "self", notice->self );
	push_branch_int( L, "entryId", notice->entryId );
	push_branch_int( L, "ownerId", notice->ownerId );
	push_branch_str( L, "name", (char*)(notice->name.block) );
	push_branch_str( L, "cmdl", (char*)(notice->cmdl.block) );
	return 1;
}

int lua_proc_glance_init( lua_State *L ) {
	int underId = luaL_optinteger(L,2,0);
	proc_glance_t *glance = luaL_checkudata(L,1,PROC_GLANCE_CLASS);
	proc_notice_t *notice;
	notice = proc_glance_init( NULL, glance, underId );
	return lua_proc_notice_info( L, notice );
}

int lua_proc_glance_term( lua_State *L ) {
	proc_glance_t *glance = luaL_checkudata(L,1,PROC_GLANCE_CLASS);
	proc_glance_term( glance );
	return 0;
}

int lua_proc_notice_next( lua_State *L ) {
	int ret = EXIT_SUCCESS;
	proc_glance_t *glance = luaL_checkudata(L,1,PROC_GLANCE_CLASS);
	proc_notice_t *notice = proc_notice_next( &ret, glance );
	return lua_proc_notice_info( L, notice );
}

int lua_proc_locate_name( lua_State *L ) {
	int ret = EXIT_SUCCESS, underId = luaL_optinteger(L,2,0);
	char const *name = luaL_optstring(L,1,NULL);
	nodes_t nodes = {0};
	proc_notice_t *notice;
	node_t i;
	
	if ( !(notice = proc_locate_name( &ret, name, &nodes, underId )) ) {
		lua_newtable(L);
		return 1;
	}
	lua_createtable( L, nodes.count + 1, 0 );
	for ( i = 0; i < nodes.count; ++i ) {
		lua_pushinteger(L,i+1);
		lua_createtable( L, 0, 5 );
		push_branch_bool( L, "self", notice->self );
		push_branch_int( L, "entryId", notice->entryId );
		push_branch_int( L, "ownerId", notice->ownerId );
		push_branch_str( L, "name", (char*)(notice->name.block) );
		push_branch_str( L, "cmdl", (char*)(notice->cmdl.block) );
		lua_settable(L,-3);
		proc_notice_zero( notice );
		++notice;
	}
	free_nodes( proc_notice_t, &ret, &nodes );
	return 1;
}
int lua_proc_glance_text( lua_State *L ) {
	lua_pushstring( L, PROC_GLANCE_CLASS );
	return 1;
}
int lua_proc_glance_grab( lua_State *L ) {
	proc_glance_t *glance =
		(proc_glance_t*)lua_newuserdata(L,sizeof(proc_glance_t));
	if ( !glance ) return 0;
	luaL_setmetatable(L,PROC_GLANCE_CLASS);
	return 1;
}

luaL_Reg lua_class_proc_glance_func_list[] = {
	{ "new", lua_proc_glance_grab },
	{ "init", lua_proc_glance_init },
	{ "next", lua_proc_notice_next },
	{ "term", lua_proc_glance_term },
	{ "tostring", lua_proc_glance_text },
{NULL}};

int lua_proc_glance_node( lua_State *L ) {
	char const *name = NULL;
	node_t num = 0, tmp;
	int i, len = 0;
	proc_glance_t *glance = luaL_checkudata(L,1,PROC_GLANCE_CLASS);
	luaL_Reg *reg;
	if ( lua_isinteger(L,2) )
		i = lua_tointeger(L,2);
	else if ( lua_isnumber(L,2) )
		i = floor( lua_tonumber(L,2) );
	else if ( lua_isstring(L,2) ) {
		name = lua_tostring(L,2);
		len = strlen(name);
		if ( sscanf( name, "%d", &i ) < len ) {
			for ( num = 0;
				lua_class_proc_glance_func_list[num].name;
				++num
			)
			{
				reg = lua_class_proc_glance_func_list + num;
				if ( strcmp(name,reg->name) == 0 )
					return reg->func(L);
			}
			return 0;
		}
	}
	else return 0;
	if ( i < 1 ) {
		if ( i == 0 ) {
			lua_pushinteger( L, glance->idNodes.count );
			return 1;
		}
		return 0;
	}
	num = i - 1;
	if ( num >= glance->idNodes.count )
		return 0;
	tmp = glance->process;
	glance->process = num;
	i = lua_proc_notice_next(L);
	glance->process = tmp;
	return i;
}

int lua_proc_glance_make( lua_State *L ) {
	(void)L;
	return 0;
}

int lua_proc_glance_free( lua_State *L ) {
	lua_proc_glance_term(L);
	return 0;
}

luaL_Reg lua_class_proc_glance_meta_list[] = {
	{ "__gc", lua_proc_glance_free },
	{ "__index",lua_proc_glance_node },
	{ "__newindex", lua_proc_glance_make },
{NULL,NULL}};

void lua_proc_create_glance_class( lua_State *L ) {
	int lib_id, meta_id;

	/* newclass = {} */
	lua_createtable(L, 0, 0);
	lib_id = lua_gettop(L);
	
	/* metatable = {} */
	luaL_newmetatable(L, PROC_GLANCE_CLASS);
	meta_id = lua_gettop(L);
	luaL_setfuncs(L, lua_class_proc_glance_meta_list, 0);
	
	/* metatable.__index = _methods */
	luaL_newlib( L, lua_class_proc_glance_func_list );
	lua_setfield( L, meta_id, "__index" );  

	/* metatable.__metatable = _meta */
	luaL_newlib( L, lua_class_proc_glance_meta_list );
	lua_setfield( L, meta_id, "__metatable");

	/* class.__metatable = metatable */
	lua_setmetatable( L, lib_id );

	/* _G["Foo"] = newclass */
	lua_setglobal(L, PROC_GLANCE_CLASS );
}

#define PROC_HANDLE_CLASS "class_proc_handle"

int lua_proc_handle_init( lua_State *L ) {
	int pid = luaL_checkinteger(L,2);
	proc_handle_t **handle = (proc_handle_t**)
		luaL_checkudata(L,1,PROC_HANDLE_CLASS);
	*handle = proc_handle_open( NULL, pid );
	lua_pushboolean( L, !!handle );
	return 1;
}

int lua_proc_handle_term( lua_State *L ) {
	proc_handle_t **handle = (proc_handle_t**)
		luaL_checkudata(L,1,PROC_HANDLE_CLASS);
	proc_handle_shut( *handle );
	*handle = NULL;
	return 0;
}
int lua_proc_glance_data( lua_State *L ) {
	proc_handle_t **handle = (proc_handle_t**)
		luaL_checkudata(L,1,PROC_HANDLE_CLASS);
	intptr_t addr = luaL_checkinteger(L,2);
	intptr_t size = luaL_checkinteger(L,3);
	uchar * array;
	if ( size < 1 ) {
		lua_newtable(L);
		return 1;
	}
	array = calloc( size, 1 );
	size = proc_glance_data( NULL, *handle, addr, array, size );
	if ( size < 1 ) {
		free(array);
		lua_newtable(L);
		return 1;
	}
	lua_createtable( L, size, 0 );
	for ( addr = 0; addr < size; ++addr ) {
		lua_pushinteger(L,addr+1);
		lua_pushinteger(L,array[addr]);
		lua_settable(L,-3);
	}
	free(array);
	return 1;
}
int lua_proc_change_data( lua_State *L ) {
	proc_handle_t **handle = (proc_handle_t**)
		luaL_checkudata(L,1,PROC_HANDLE_CLASS);
	intptr_t addr = luaL_checkinteger(L,2);
	intptr_t size, i;
	uchar * array;
	if ( !lua_istable(L,3) ) {
		lua_pushinteger(L,0);
		return 1;
	}
	lua_len(L,3);
	size = lua_tointeger(L,-1);
	if ( size < 1 ) {
		lua_pushinteger(L,0);
		return 1;
	}
	array = calloc( size, 1 );
	for ( i = 0; i < size; ++i ) {
		lua_pushinteger(L,i+1);
		lua_gettable(L,3);
		array[i] = luaL_checkinteger(L,-1);
		lua_pop(L,1);
	}
	size = proc_change_data( NULL, *handle, addr, array, size );
	free(array);
	if ( size < 1 ) {
		lua_pushinteger(L,0);
		return 1;
	}
	lua_pushinteger(L,0);
	return 1;
}

int lua_proc_aobscan( lua_State *L ) {
	proc_handle_t **handle = (proc_handle_t**)
		luaL_checkudata(L,1,PROC_HANDLE_CLASS);
	char path[32] = {0};
	int fd = -1, c;
	char const *array_str = luaL_checkstring( L, 2 );
	node_t bytes = 0, i = 0, leng = strlen(array_str), count;
	uchar *array = calloc(leng,1);
	intptr_t from = luaL_optinteger(L,3,0);
	intptr_t upto = luaL_optinteger(L,4,INTPTR_MAX);
	_Bool writable = 1;
	if ( lua_isboolean(L,5) )
		writable = lua_toboolean(L,5);
	if ( !array ) {
		lua_newtable(L);
		return 1;
	}
	++leng;
	for ( i = 0; i < leng; ++i ) {
		c = *array_str;
		if ( c == 0 ) {
			++bytes;
			break;
		}
		if ( isspace(c) ) {
			++bytes;
			continue;
		}
		if ( array[bytes] & UCHAR_MAX << CHAR_BIT ) {
			luaL_dostring(L,"print(debug.traceback())\n"
				"error('Beyond scope of native byte')");
			lua_newtable(L);
			return 1;
		}
		array[bytes] <<= CHAR_BIT;
		if ( c >= '0' && c <= '9' )
			array[bytes] |= (c - '0');
		else if ( c >= 'A' && c <= 'F' )
			array[bytes] |= (c - 'A');
		else if ( c >= 'a' && c <= 'f' )
			array[bytes] |= (c - 'a');
		else {
			luaL_dostring(L,"print(debug.traceback())\n"
				"error('Bytes only use Hexadecimal')");
			lua_newtable(L);
			return 1;
		}
	}
	--leng;
	c = time(NULL);
	sprintf( path, "/tmp/%06X.aobscan", rand_r((unsigned int *)(&c)) );
	fd = open( path, O_CREAT | O_RDWR );
	count = proc_aobscan(
		NULL, fd, *handle, array, bytes, from, upto, writable );
	free(array);
	for ( i = 0; i < count; ++i ) {
		sprintf( path, "%p", array );
		lua_pushstring( L, path );
	}
	return 1;
}

int lua_proc_handle_text( lua_State *L ) {
	lua_pushstring( L, PROC_HANDLE_CLASS );
	return 1;
}
int lua_proc_handle_grab( lua_State *L ) {
	proc_handle_t **handle =
		(proc_handle_t**)lua_newuserdata(L,sizeof(proc_handle_t*));
	if ( !handle ) return 0;
	luaL_setmetatable(L,PROC_HANDLE_CLASS);
	return 1;
}

luaL_Reg lua_class_proc_handle_func_list[] = {
	{ "new", lua_proc_handle_grab },
	{ "init", lua_proc_handle_init },
	{ "read", lua_proc_glance_data },
	{ "write", lua_proc_change_data },
	{ "aobscan", lua_proc_aobscan },
	{ "term", lua_proc_handle_term },
	{ "tostring", lua_proc_handle_text },
{NULL}};

int lua_proc_handle_node( lua_State *L ) {
	char const *name = NULL;
	node_t num = 0;
	int i, len = 0;
	luaL_Reg *reg;
	if ( lua_isstring(L,2) ) {
		name = lua_tostring(L,2);
		len = strlen(name);
		if ( sscanf( name, "%d", &i ) < len ) {
			for ( num = 0;
				lua_class_proc_handle_func_list[num].name;
				++num
			)
			{
				reg = lua_class_proc_handle_func_list + num;
				if ( strcmp(name,reg->name) == 0 )
					return reg->func(L);
			}
			return 0;
		}
	}
	return 0;
}

int lua_proc_handle_make( lua_State *L ) {
	(void)L;
	return 0;
}

int lua_proc_handle_free( lua_State *L ) {
	lua_proc_handle_term(L);
	return 0;
}

luaL_Reg lua_class_proc_handle_meta_list[] = {
	{ "__gc", lua_proc_handle_free },
	{ "__index",lua_proc_handle_node },
	{ "__newindex", lua_proc_handle_make },
{NULL,NULL}};

void lua_proc_create_handle_class( lua_State *L ) {
	int lib_id, meta_id;

	/* newclass = {} */
	lua_createtable(L, 0, 0);
	lib_id = lua_gettop(L);
	
	/* metatable = {} */
	luaL_newmetatable(L, PROC_HANDLE_CLASS);
	meta_id = lua_gettop(L);
	luaL_setfuncs(L, lua_class_proc_handle_meta_list, 0);
	
	/* metatable.__index = _methods */
	luaL_newlib( L, lua_class_proc_handle_func_list );
	lua_setfield( L, meta_id, "__index" );  

	/* metatable.__metatable = _meta */
	luaL_newlib( L, lua_class_proc_handle_meta_list );
	lua_setfield( L, meta_id, "__metatable");

	/* class.__metatable = metatable */
	lua_setmetatable( L, lib_id );

	/* _G["Foo"] = newclass */
	lua_setglobal(L, PROC_HANDLE_CLASS );
}
void lua_create_proc_classes( lua_State *L ) {
	lua_proc_create_glance_class(L);
	lua_proc_create_handle_class(L);
}
