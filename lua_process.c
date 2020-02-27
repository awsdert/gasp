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
	
	if ( !(notice = proc_locate_name( &ret, name, &nodes, underId, 0 )) ) {
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

int lua_proc_glance_leng( lua_State *L ) {
	proc_glance_t *glance = luaL_checkudata(L,1,PROC_GLANCE_CLASS);
	lua_pushinteger(L,glance->idNodes.count);
	return 1;
}

luaL_Reg lua_class_proc_glance_meta_list[] = {
	{ "__gc", lua_proc_glance_free },
	{ "__len", lua_proc_glance_leng },
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

typedef struct lua_proc_handle {
	proc_handle_t *handle;
	_Bool nth_scan;
	node_t scan_count;
	tscan_t tscan;
	node_t scan_instance;
	nodes_t bytes;
} lua_proc_handle_t;

void lua_proc_handle_checkrdwr( lua_proc_handle_t *handle ) {
	if ( !handle || !(handle->tscan.threadmade) ) return;
	handle->tscan.wants2read = 1;
	while (	!(handle->tscan.ready2wait) ) sleep(1);
}

void lua_proc_handle_shutfiles( lua_proc_handle_t *handle )
{
	handle->tscan.wants2free = 1;
	/* sleep call is needed here to ensure system passes execution to
	 * the other thread when it exists on the same CPU core */
	while ( handle->tscan.threadmade ) sleep(1);
	shut_dump_files( handle->tscan.prev_dump );
	memset( &(handle->tscan.prev_dump), -1, sizeof(dump_t) );
	shut_dump_files( handle->tscan.next_dump );
	memset( &(handle->tscan.next_dump), -1, sizeof(dump_t) );
}

bool lua_proc_handle_undo( lua_proc_handle_t *handle, node_t count, bool all )
{
	char const *GASP_PATH = getenv("GASP_PATH");
	char *path;
	int ret = EXIT_SUCCESS;
	node_t i;
	if ( !(path = more_space(
		&ret, &(handle->tscan.scan.space),
		strlen( GASP_PATH ) + UCHAR_MAX))
	) {
		ERRMSG( ret, "Couldn't allocate memory for scan path" );
		return 0;
	}
	if ( all ) {
		sprintf( path, "%s/scans/%lu",
				GASP_PATH, (ulong)(handle->scan_instance) );
		if ( access( path, F_OK ) == 0 ) {
			sprintf( path, "rm -r \"%s/scans/%lu\"",
					GASP_PATH, (ulong)(handle->scan_instance) );
			fprintf( stderr, "%s\n", path );
			system( path );
		}
		handle->scan_instance = 0;
		handle->scan_count = 0;
		handle->nth_scan = 0;
		return 1;
	}
	for ( i = handle->scan_count - 1; i > count; --i ) {
		sprintf( path, "rm -r \"%s/scans/%lu/%lu*\"",
				GASP_PATH, (ulong)(handle->scan_instance), (ulong)i );
		fprintf( stderr, "%s\n", path );
		system( path );
	}
	handle->scan_count = count;
	return 1;
}

int lua_proc_handle_term( lua_State *L ) {
	lua_proc_handle_t *handle = (lua_proc_handle_t*)
		luaL_checkudata(L,1,PROC_HANDLE_CLASS);
	lua_proc_handle_undo( handle, 0, 1 );
	proc_handle_shut( handle->handle );
	handle->handle = NULL;
	free_space( NULL, &(handle->tscan.scan.space) );
	free_nodes( uchar, NULL, &(handle->bytes) );
	return 0;
}

int lua_proc_handle_valid( lua_State *L ) {
	lua_proc_handle_t *handle = (lua_proc_handle_t*)
		luaL_checkudata(L,1,PROC_HANDLE_CLASS);
	lua_pushboolean( L, handle->handle != NULL );
	return 1;
}

int lua_proc_handle_init( lua_State *L ) {
	int pid = luaL_checkinteger(L,2), ret = EXIT_SUCCESS;
	lua_proc_handle_t *handle = (lua_proc_handle_t*)
		luaL_checkudata(L,1,PROC_HANDLE_CLASS);
	if ( handle->handle ) lua_proc_handle_term( L );
	handle->handle = proc_handle_open( &ret, pid );
	return lua_proc_handle_valid(L);
}

int lua_proc_handle_glance_data( lua_State *L ) {
	lua_proc_handle_t *handle = (lua_proc_handle_t*)
		luaL_checkudata(L,1,PROC_HANDLE_CLASS);
	intptr_t addr = luaL_checkinteger(L,2);
	intptr_t size = luaL_checkinteger(L,3);
	uchar * array;
	if ( !(handle->handle) || size < 1 ) {
		lua_newtable(L);
		return 1;
	}
	array = calloc( size, 1 );
	if ( !array ) {
		ERRMSG( errno, "Couldn't allocate memory" );
		lua_newtable(L);
		return 1;
	}
	lua_proc_handle_checkrdwr(handle);
	size = proc_glance_data( NULL, handle->handle, addr, array, size );
	handle->tscan.wants2read = 0;
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
int lua_proc_handle_change_data( lua_State *L ) {
	lua_proc_handle_t *handle = (lua_proc_handle_t*)
		luaL_checkudata(L,1,PROC_HANDLE_CLASS);
	intptr_t addr = luaL_checkinteger(L,2), size;
	uchar * array = NULL;
	nodes_t *nodes;
	
	if ( !(handle->handle) ) {
		lua_pushinteger(L,0);
		return 1;
	}
	nodes = &(handle->bytes);
	
	if ( !(array = lua_extract_bytes( NULL, L, 5, nodes)) ) {
		lua_pushinteger(L,0);
		return 1;
	}
	
	lua_proc_handle_checkrdwr(handle);
	
	size = proc_change_data( NULL, handle->handle, addr, array, nodes->count );
	
	handle->tscan.wants2read = 0;

	lua_pushinteger(L,(size > 0) ? size : 0);
	return 1;
}

int lua_proc_handle_done_scans( lua_State *L ) {
	lua_proc_handle_t *handle = (lua_proc_handle_t*)
		luaL_checkudata(L,1,PROC_HANDLE_CLASS);
	lua_pushinteger( L, handle->tscan.done_scans );
	return 1;
}

int lua_proc_handle_scan_done_upto( lua_State *L ) {
	lua_proc_handle_t *handle = (lua_proc_handle_t*)
		luaL_checkudata(L,1,PROC_HANDLE_CLASS);
	lua_pushinteger( L, handle->tscan.done_upto );
	return 1;
}
int lua_proc_handle_doing_scan( lua_State *L ) {
	lua_proc_handle_t *handle = (lua_proc_handle_t*)
		luaL_checkudata(L,1,PROC_HANDLE_CLASS);
	lua_pushboolean( L, handle->tscan.threadmade );
	return 1;
}

int lua_proc_handle_scan_found( lua_State *L ) {
	lua_proc_handle_t *handle = (lua_proc_handle_t*)
		luaL_checkudata(L,1,PROC_HANDLE_CLASS);
	lua_pushinteger( L, handle->tscan.scan.count );
	return 1;
}

int lua_proc_handle__get_scan_list(
	lua_State *L, lua_proc_handle_t *handle, node_t limit
)
{
	intptr_t addr = 0;
	node_t i, count = 0;
	lua_proc_handle_checkrdwr(handle);
	count = handle->tscan.scan.count;
	lua_pushinteger( L, handle->tscan.done_upto );
	if ( count > limit ) count = limit;
	if ( !count ) {
		lua_newtable( L );
		lua_pushinteger( L, 0 );
		return 3;
	}
	lua_createtable( L, count, 0 );
	gasp_lseek( handle->tscan.next_dump.addr_fd, 0, SEEK_SET );
	for ( i = 0; i < count; ++i ) {
		gasp_read( handle->tscan.next_dump.addr_fd, &addr, sizeof(void*) );
		lua_pushinteger( L, i+1 );
		lua_pushinteger( L, addr );
		lua_settable(L,-3);
	}
	gasp_lseek( handle->tscan.next_dump.addr_fd, 0, SEEK_END );
	lua_pushinteger( L, count );
	handle->tscan.wants2read = 0;
	return 3;
}

int lua_proc_handle_get_scan_list( lua_State *L ) {
	return lua_proc_handle__get_scan_list(
		L,
		(lua_proc_handle_t*)luaL_checkudata(L,1,PROC_HANDLE_CLASS),
		luaL_optinteger( L, 2, 100 ) );
}

int lua_proc_handle_aobscan( lua_State *L ) {
	lua_proc_handle_t *handle = (lua_proc_handle_t*)
		luaL_checkudata(L,1,PROC_HANDLE_CLASS);
	char *path;
	char const *GASP_PATH = getenv("GASP_PATH");
	int ret, index = 4;
	node_t i = 0;
	uchar *array;
	intptr_t from = luaL_optinteger(L,index+1,0);
	intptr_t upto = luaL_optinteger(L,index+2,INTPTR_MAX);
	_Bool writable = lua_toboolean(L,index+3);
	node_t limit = luaL_optinteger(L,index+4,100), count;
	_Bool all;
	if ( limit > LONG_MAX ) limit = 100;
	
	if ( !(handle->handle) ) {
		lua_newtable(L);
		lua_pushinteger(L,0);
		return 2;
	}
	
	if ( handle->tscan.threadmade )
		return lua_proc_handle__get_scan_list(L, handle, limit);
	
	if ( !(path = more_space(
		&ret, &(handle->tscan.scan.space), strlen( GASP_PATH ) + UCHAR_MAX))
	) {
		ERRMSG( ret, "Couldn't allocate memory for scan path" );
		lua_newtable(L);
		lua_pushinteger(L,0);
		return 2;
	}
	
	count = luaL_optinteger(L,index+5,handle->scan_count);
	all = luaL_optinteger(L,index+6,0);
	if ( all ) count = 0;
	if ( count < handle->scan_count || all )
		lua_proc_handle_undo( handle, count, all );
	else if ( count > handle->scan_count )
		return 0;
	if ( !(array = lua_extract_bytes(
		NULL, L, index, &(handle->bytes) ))
	)
	{
		fprintf( stderr, "2: %s\n", GASP_PATH );
		lua_newtable(L);
		lua_pushinteger(L,0);
		return 2;
	}
	
	if ( access(GASP_PATH, F_OK) != 0 ) {
		sprintf( path, "mkdir %s", GASP_PATH );
		system(path);
	}
	
	sprintf( path, "%s/scans", GASP_PATH );
	if ( access(path, F_OK) != 0 ) {
		sprintf( path, "mkdir %s/scans", GASP_PATH );
		system(path);
	}
	
	if ( !(handle->nth_scan) ) {
		for ( handle->scan_instance = 0;
			handle->scan_instance < LONG_MAX;
			handle->scan_instance++
		)
		{
			sprintf( path, "%s/scans/%lu",
				GASP_PATH, (ulong)(handle->scan_instance) );
			if ( access( path, F_OK ) != 0 ) {
				sprintf( path, "mkdir %s/scans/%lu",
					GASP_PATH, (ulong)(handle->scan_instance) );
				system(path);
				break;
			}
		}
		handle->nth_scan = 1;
	}
	
	handle->tscan.handle = handle->handle;
	handle->tscan.array = array;
	handle->tscan.bytes = handle->bytes.count;
	handle->tscan.from = from;
	handle->tscan.upto = upto;
	handle->tscan.writeable = writable;
	handle->tscan.done_upto = 0;
	handle->tscan.done_scans = count;
	
	if ( count ) {
		if ( (ret = open_dump_files(
			&(handle->tscan.prev_dump), &(handle->tscan.scan),
			handle->scan_instance, count )) != EXIT_SUCCESS ) {
			ERRMSG( ret, "Couldn't open input file" );
			lua_newtable(L);
			lua_pushinteger(L,0);
			return 2;
		}
		if ( (ret = open_dump_files(
			&(handle->tscan.next_dump), &(handle->tscan.scan),
			handle->scan_instance, count )) != EXIT_SUCCESS ) {
			ERRMSG( ret, "Couldn't open output file" );
			lua_newtable(L);
			lua_pushinteger(L,0);
			return 2;
		}
#if 1
		pthread_create( &(handle->tscan.thread), NULL,
			bytescanner, &(handle->tscan) );
#else
		count = proc_aobscan(
			NULL,
			handle->tscan.prev_dump.addr_fd,
			handle->tscan.next_dump.addr_fd,
			&(handle->tscan->scan), handle->handle,
			array, handle->bytes.count, from, upto, writable );
#endif
	}
	else {
		if ( (ret = open_dump_files(
			&(handle->tscan.next_dump), &(handle->tscan.scan),
			handle->scan_instance, 0 )) != EXIT_SUCCESS ) {
			ERRMSG( ret, "Couldn't open output file" );
			lua_newtable(L);
			lua_pushinteger(L,0);
			return 2;
		}
#if 1
		pthread_create( &(handle->tscan.thread), NULL,
			bytescanner, &(handle->tscan) );
#else
		count = proc_aobinit(
			NULL,
			handle->tscan.next_dump.addr_fd,
			&(handle->tscan->scan), handle->handle,
			array, handle->bytes.count, from, upto, writable );
#endif
	}
	if ( count > limit ) count = limit;
	lua_createtable( L, count, 0 );
	gasp_lseek( handle->tscan.next_dump.addr_fd, 0, SEEK_SET );
	for ( i = 0; i < count; ++i ) {
		gasp_read( handle->tscan.next_dump.addr_fd, &from, sizeof(void*) );
		lua_pushinteger( L, i+1 );
		lua_pushinteger( L, from );
		lua_settable(L,-3);
	}
	lua_pushinteger( L, count - 1 );
	handle->scan_count++;
	return 2;
}

int lua_proc_handle_text( lua_State *L ) {
	lua_pushstring( L, PROC_HANDLE_CLASS );
	return 1;
}
int lua_proc_handle_grab( lua_State *L ) {
	lua_proc_handle_t *handle =
		(lua_proc_handle_t*)lua_newuserdata(L,sizeof(lua_proc_handle_t));
	if ( !handle ) return 0;
	memset( handle, 0, sizeof(lua_proc_handle_t) );
	luaL_setmetatable(L,PROC_HANDLE_CLASS);
	return 1;
}

luaL_Reg lua_class_proc_handle_func_list[] = {
	{ "new", lua_proc_handle_grab },
	{ "init", lua_proc_handle_init },
	{ "valid", lua_proc_handle_valid },
	{ "glance", lua_proc_handle_glance_data },
	{ "change", lua_proc_handle_change_data },
	{ "aobscan", lua_proc_handle_aobscan },
	{ "doing_scan", lua_proc_handle_doing_scan },
	{ "scan_found", lua_proc_handle_scan_found },
	{ "scan_done_upto", lua_proc_handle_scan_done_upto },
	{ "done_scans", lua_proc_handle_done_scans },
	{ "get_scan_list", lua_proc_handle_get_scan_list },
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
#define PROC_MEMORY_CLASS "class_proc_memory"

int lua_proc_memory_term( lua_State *L ) {
	space_t *space = (space_t*)
		luaL_checkudata(L,1,PROC_MEMORY_CLASS);
	less_space( NULL, space, 0 );
	return 0;
}

int lua_proc_memory_init( lua_State *L ) {
	int want = luaL_checkinteger(L,2);
	space_t *space = (space_t*)
		luaL_checkudata(L,1,PROC_MEMORY_CLASS);
	if ( space ) lua_proc_memory_term( L );
	lua_pushboolean( L, !!more_space( NULL, space, want ) );
	return 1;
}

int lua_proc_memory_text( lua_State *L ) {
	lua_pushstring( L, PROC_MEMORY_CLASS );
	return 1;
}
int lua_proc_memory_grab( lua_State *L ) {
	space_t *space = lua_newuserdata(L,sizeof(space_t));
	if ( !space ) return 0;
	space->given = 0;
	space->block = NULL;
	luaL_setmetatable(L,PROC_MEMORY_CLASS);
	return 1;
}

luaL_Reg lua_class_proc_memory_func_list[] = {
	{ "new", lua_proc_memory_grab },
	{ "init", lua_proc_memory_init },
	{ "term", lua_proc_memory_term },
	{ "tostring", lua_proc_memory_text },
{NULL}};

int lua_proc_memory_node( lua_State *L ) {
	char const *name = NULL;
	space_t *space = (space_t*)
		luaL_checkudata(L,1,PROC_MEMORY_CLASS);
	node_t num = 0;
	int i, len = 0;
	luaL_Reg *reg;
	if ( lua_isinteger(L,2) ) {
		i = lua_tointeger(L,2);
		if ( (size_t)i < space->given ) {
			lua_pushinteger(L,((uchar*)(space->block))[i]);
			return 1;
		}
		return 0;
	}
	if ( lua_isstring(L,2) ) {
		name = lua_tostring(L,2);
		if ( strcmp(name,"__len") == 0 ) {
			lua_pushinteger( L, space->given );
			return 1;
		}
		len = strlen(name);
		
		if ( sscanf( name, "%d", &i ) < len ) {
			for ( num = 0;
				lua_class_proc_memory_func_list[num].name;
				++num
			)
			{
				reg = lua_class_proc_memory_func_list + num;
				if ( strcmp(name,reg->name) == 0 )
					return reg->func(L);
			}
			return 0;
		}
	}
	return 0;
}

int lua_proc_memory_make( lua_State *L ) {
	(void)L;
	return 0;
}

int lua_proc_memory_free( lua_State *L ) {
	lua_proc_memory_term(L);
	return 0;
}

luaL_Reg lua_class_proc_memory_meta_list[] = {
	{ "__gc", lua_proc_memory_free },
	{ "__index",lua_proc_memory_node },
	{ "__newindex", lua_proc_memory_make },
{NULL,NULL}};

void lua_proc_create_memory_class( lua_State *L ) {
	int lib_id, meta_id;

	/* newclass = {} */
	lua_createtable(L, 0, 0);
	lib_id = lua_gettop(L);
	
	/* metatable = {} */
	luaL_newmetatable(L, PROC_MEMORY_CLASS);
	meta_id = lua_gettop(L);
	luaL_setfuncs(L, lua_class_proc_memory_meta_list, 0);
	
	/* metatable.__index = _methods */
	luaL_newlib( L, lua_class_proc_memory_func_list );
	lua_setfield( L, meta_id, "__index" );  

	/* metatable.__metatable = _meta */
	luaL_newlib( L, lua_class_proc_memory_meta_list );
	lua_setfield( L, meta_id, "__metatable");

	/* class.__metatable = metatable */
	lua_setmetatable( L, lib_id );

	/* _G["Foo"] = newclass */
	lua_setglobal(L, PROC_MEMORY_CLASS );
}
void lua_create_proc_classes( lua_State *L ) {
	lua_proc_create_glance_class(L);
	lua_proc_create_handle_class(L);
}
