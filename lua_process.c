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

bool lua_proc_handle__end_scan( tscan_t *tscan ) {
	int ret = SIGSTOP;
	_Bool success = 1;
	if ( tscan->threadmade ) {
		success = gasp_tpollo( tscan->scan_pipes, SIGKILL, -1 );
		if ( success && tscan->threadmade )
			success = gasp_tpolli( tscan->main_pipes, &ret, -1 );
	}
	return success;
}

bool lua_proc_handle__set_rdwr(
	lua_proc_handle_t *handle, bool want2rdwr
)
{
	int ret = want2rdwr ? SIGSTOP : SIGCONT;
	tscan_t *tscan;
	
	if ( !handle ) return 0;
	tscan = &(handle->tscan);
	tscan->wants2rdwr = want2rdwr;
	if ( tscan->threadmade &&
		gasp_tpollo( tscan->scan_pipes, ret, -1 )
	)
	{
		if ( want2rdwr )
			return gasp_tpolli( tscan->main_pipes, &ret, -1 );
	}
	return 1;
}

int lua_proc_handle__ret_empty_list( lua_State *L, node_t done, node_t found ) {
	lua_pushinteger(L,done);
	lua_pushinteger(L,found);
	lua_newtable( L );
	return 3;
}

int lua_proc_handle__get_scan_list(
	lua_State *L, lua_proc_handle_t *handle, node_t limit
)
{
	int ret;
	node_t i, a, stop, count = 0;
	dump_t *dump, _dump = {0};
	//proc_mapped_t _pmap, _nmap, *pmap, *nmap;
	uchar *used;
	intmax_t ipos, upos, dpos;
	uintmax_t addr, minus;
	tscan_t *tscan;
	
	if ( !L )
		return 0;

	if ( !handle )
		return lua_proc_handle__ret_empty_list(L,0,0);
	tscan = &(handle->tscan);
	if ( tscan->dumping )
		return lua_proc_handle__ret_empty_list(L,tscan->done_scans,tscan->found);
	minus = tscan->bytes - 1;
	/* Initialise pointer */
	dump = &(tscan->dump[1]);
	/* Pause thread */
	if ( !lua_proc_handle__set_rdwr(handle,1) )
		return lua_proc_handle__ret_empty_list(L,tscan->done_scans,tscan->found);
	/* Grab current state */
	_dump = *dump;
	ipos = gasp_lseek( dump->info_fd, 0, SEEK_CUR );
	upos = gasp_lseek( dump->used_fd, 0, SEEK_CUR );
	dpos = gasp_lseek( dump->data_fd, 0, SEEK_CUR );
	
	/* Grab current address and amount found */
	lua_pushinteger( L, handle->tscan.done_upto );
	lua_pushinteger( L, (count = handle->tscan.found) );
	
	/* Clamp down the number we use to what was declared as the limit */
	if ( count > limit ) count = limit;
	
	/* Pre-create the entire table */
	lua_createtable( L, count, 0 );
	
	if ( !count ) {
		/* Un-Pause thread and return */
		lua_proc_handle__set_rdwr(handle,0);
		return 3;
	}
	
	/* Move pointers to where we want them*/
	dump_files_reset_offsets( dump, 1 );
	
	/* Read through scanned regions for addresses to declare */
	while ( dump_files_glance_stored( &ret, dump, tscan->bytes )
		|| ret == ERANGE )
	{
		stop = dump->size - minus;
		used = dump->used;
		addr = dump->addr;
		for ( a = 0; a < stop; ++a, ++addr ) {
			if ( used[a] ) {
				lua_pushinteger( L, ++i );
				lua_pushinteger( L, addr );
				lua_settable(L,-3);
				/* Stop when we hit the allocated amount */
				if ( i == count ) goto done;
			}
		}
	}
	
	done:
	/* Restore prior state */
	*dump = _dump;
	gasp_lseek( dump->info_fd, ipos, SEEK_SET );
	gasp_lseek( dump->used_fd, upos, SEEK_SET );
	gasp_lseek( dump->data_fd, dpos, SEEK_SET );
	
	lua_proc_handle__set_rdwr(handle,0);
	return 3;
}
bool lua_proc_handle__nth_scan( lua_proc_handle_t *handle ) {
	char *path;
	char const *GASP_PATH = getenv("GASP_PATH");
	tscan_t *tscan;
	space_t space = {0}, path_space = {0};
	_Bool nth = 0;
	
	if ( !handle ) return 0;
	if ( handle->nth_scan ) return 1;
	
	tscan = &(handle->tscan);
	
	
	if ( !(path = more_space(
		&(tscan->ret), &path_space, strlen( GASP_PATH ) + UCHAR_MAX))
	) {
		ERRMSG( tscan->ret, "Couldn't allocate memory for scan path" );
		goto fail;
	}
	
	if ( gasp_mkdir( &space, GASP_PATH ) != EXIT_SUCCESS )
		goto fail;
	
	sprintf( path, "%s/scans", GASP_PATH );
	if ( gasp_mkdir( &space, path ) != EXIT_SUCCESS )
		goto fail;
	
	if ( !(handle->nth_scan) ) {
		for ( handle->scan_instance = 0;
			handle->scan_instance < LONG_MAX;
			handle->scan_instance++
		)
		{
			sprintf( path, "%s/scans/%lu",
				GASP_PATH, (ulong)(handle->scan_instance) );
			if ( gasp_isdir( path ) != EXIT_SUCCESS
				&& gasp_mkdir( &space, path ) == EXIT_SUCCESS )
				goto done;
		}
		goto fail;
	}
	done:
	nth = 1;
	fail:
	free_space(NULL,&path_space);
	free_space(NULL,&space);
	return nth;
}

void lua_proc_handle_shutfiles( lua_proc_handle_t *handle )
{
	int ret = SIGSTOP;
	tscan_t *tscan = &(handle->tscan);
	
	if ( tscan->pipesmade ) {
		if ( tscan->threadmade ) {
			(void)gasp_tpollo( tscan->scan_pipes, SIGKILL, -1 );
			(void)gasp_tpolli( tscan->main_pipes, &ret, -1 );
		}
		close( tscan->main_pipes[0] );
		close( tscan->main_pipes[1] );
		close( tscan->scan_pipes[0] );
		close( tscan->scan_pipes[1] );
		tscan->main_pipes[0] = tscan->main_pipes[1] = -1;
		tscan->scan_pipes[0] = tscan->scan_pipes[1] = -1;
		tscan->pipesmade = 0;
	}
	
	/* sleep call is needed here to ensure system passes execution to
	 * the other thread when it exists on the same CPU core */
	dump_files_shut( &(handle->tscan.dump[0]) );
	dump_files_shut( &(handle->tscan.dump[1]) );
}

bool lua_proc_handle_undo( lua_proc_handle_t *handle, node_t count, bool all )
{
	char const *GASP_PATH = getenv("GASP_PATH");
	char *path;
	int ret = EXIT_SUCCESS;
	node_t i;
	space_t space = {0};
	_Bool success = 0;
	if ( !(path = more_space(
		&ret, &space, strlen( GASP_PATH ) + UCHAR_MAX))
	) {
		ERRMSG( ret, "Couldn't allocate memory for scan path" );
		goto fail;
	}
	if ( all ) {
		sprintf( path, "%s/scans/%lu",
				GASP_PATH, (ulong)(handle->scan_instance) );
		gasp_rmdir( &space, path, 1 );
		handle->scan_instance = 0;
		count = 0;
		handle->nth_scan = 0;
		goto done;
	}
	for ( i = handle->scan_count - 1; i > count; --i ) {
		sprintf( path, "rm \"%s/scans/%lu/%lu*\"",
				GASP_PATH, (ulong)(handle->scan_instance), (ulong)i );
		fprintf( stderr, "%s\n", path );
		system( path );
	}
	done:
	success = 1;
	handle->scan_count = count;
	fail:
	free_space( NULL, &space );
	return success;
}

int lua_proc_handle_term( lua_State *L ) {
	lua_proc_handle_t *handle = (lua_proc_handle_t*)
		luaL_checkudata(L,1,PROC_HANDLE_CLASS);
	lua_proc_handle_undo( handle, 0, 1 );
	proc_handle_shut( handle->handle );
	handle->handle = NULL;
	dump_files_shut( &(handle->tscan.dump[0]) );
	dump_files_shut( &(handle->tscan.dump[1]) );
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
	uintmax_t addr = luaL_checkinteger(L,2);
	uintmax_t size = luaL_checkinteger(L,3);
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
	lua_proc_handle__set_rdwr(handle,1);
	size = proc_glance_data( NULL, handle->handle, addr, array, size );
	lua_proc_handle__set_rdwr(handle,0);
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
	uintmax_t addr = luaL_checkinteger(L,2), size;
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
	
	//lua_proc_handle__set_rdwr(handle,1);
	size = proc_change_data( NULL, handle->handle, addr, array, nodes->count );
	//lua_proc_handle__set_rdwr(handle,0);

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
int lua_proc_handle_thread_active( lua_State *L ) {
	lua_proc_handle_t *handle = (lua_proc_handle_t*)
		luaL_checkudata(L,1,PROC_HANDLE_CLASS);
	lua_pushboolean( L, handle->tscan.threadmade );
	return 1;
}
int lua_proc_handle_scanning( lua_State *L ) {
	lua_proc_handle_t *handle = (lua_proc_handle_t*)
		luaL_checkudata(L,1,PROC_HANDLE_CLASS);
	lua_pushboolean( L, handle->tscan.scanning );
	return 1;
}
int lua_proc_handle_dumping( lua_State *L ) {
	lua_proc_handle_t *handle = (lua_proc_handle_t*)
		luaL_checkudata(L,1,PROC_HANDLE_CLASS);
	lua_pushboolean( L, handle->tscan.dumping );
	return 1;
}

int lua_proc_handle_scan_found( lua_State *L ) {
	lua_proc_handle_t *handle = (lua_proc_handle_t*)
		luaL_checkudata(L,1,PROC_HANDLE_CLASS);
	lua_pushinteger( L, handle->tscan.found );
	return 1;
}

int lua_proc_handle_get_scan_list( lua_State *L ) {
	return lua_proc_handle__get_scan_list(
		L,
		(lua_proc_handle_t*)luaL_checkudata(L,1,PROC_HANDLE_CLASS),
		luaL_optinteger( L, 3, 100 ) );
}

bool lua_proc_handle_prep_scan(
	lua_proc_handle_t *handle,
	node_t done,
	node_t last_found,
	uintmax_t from, uintmax_t upto, bool writable, int zero
)
{
	int ret = EXIT_SUCCESS;
	tscan_t *tscan;
	dump_t *dump;
	if ( !handle ) return 0;
	
	tscan = &(handle->tscan);
	
	if ( !(handle->handle) || tscan->threadmade )
		return 0;
	
	handle->nth_scan = lua_proc_handle__nth_scan( handle );
	
	lua_proc_handle_undo( handle, done, !done );
	
	dump = &(tscan->dump[0]);
	(void)memset( dump, 0, sizeof(dump_t) );
	dump->pmap = &(dump->mapped[0]);
	dump->nmap = &(dump->mapped[1]);
	
	dump = &(tscan->dump[1]);
	(void)memset( dump, 0, sizeof(dump_t) );
	dump->pmap = &(dump->mapped[0]);
	dump->nmap = &(dump->mapped[1]);
	
	tscan->handle = handle->handle;
	tscan->bytes = handle->bytes.count;
	tscan->array = tscan->bytes ? handle->bytes.space.block : NULL;
	tscan->from = from;
	tscan->upto = upto;
	tscan->writeable = writable;
	tscan->done_upto = 0;
	tscan->done_scans = done;
	tscan->dumping = 0;
	tscan->scanning = 0;
	tscan->threadmade = 0;
	tscan->found = 0;
	tscan->last_found = last_found;
	tscan->main_pipes[0] = -1;
	tscan->main_pipes[1] = -1;
	tscan->scan_pipes[0] = -1;
	tscan->scan_pipes[1] = -1;
	tscan->zero = zero;
	
	if ( pipe2( tscan->main_pipes, 0 ) == -1 )
		return 0;
	if ( pipe2( tscan->scan_pipes, 0 ) == -1 )
	{
		close( tscan->main_pipes[0] );
		close( tscan->main_pipes[1] );
		return 0;
	}
	
	if ( (ret = dump_files_open(
		dump, handle->scan_instance, done )) != EXIT_SUCCESS ) {
		close( tscan->main_pipes[0] );
		close( tscan->main_pipes[1] );
		close( tscan->scan_pipes[0] );
		close( tscan->scan_pipes[1] );
		ERRMSG( ret, "Couldn't open output file" );
		return 0;
	}
	
	if ( done && (ret = dump_files_open(
		&(tscan->dump[0]), handle->scan_instance, done ))
		!= EXIT_SUCCESS
	)
	{
		close( tscan->main_pipes[0] );
		close( tscan->main_pipes[1] );
		close( tscan->scan_pipes[0] );
		close( tscan->scan_pipes[1] );
		dump_files_shut( dump );
		ERRMSG( ret, "Couldn't open input file" );
		return 0;
	}
	
	tscan->pipesmade = 1;
	return 1;
}

int lua_proc_handle_dump( lua_State *L ) {
	lua_proc_handle_t *handle = (lua_proc_handle_t*)
		luaL_checkudata(L,1,PROC_HANDLE_CLASS);
	tscan_t *tscan;
	uintmax_t from = luaL_optinteger(L,2,0);
	uintmax_t upto = luaL_optinteger(L,3,INTPTR_MAX);
	_Bool writable = lua_toboolean(L,4);
	tscan =  &(handle->tscan);
	
	handle->bytes.count = 0;
	
	if ( !lua_proc_handle_prep_scan(
			handle, 0, 0, from, upto, writable, 0 )
	)
	{
		lua_pushboolean(L,0);
		return 1;
	}
	
	pthread_create(
		&(tscan->thread), NULL, proc_handle_dumper, &(handle->tscan) );
	lua_pushboolean(L, 1 );
	handle->scan_count++;
	return 1;
}

int lua_proc_handle_killscan( lua_State *L ) {
	lua_proc_handle_t *handle = (lua_proc_handle_t*)
		luaL_checkudata(L,1,PROC_HANDLE_CLASS);
	lua_pushboolean( L,
		lua_proc_handle__end_scan( &(handle->tscan) )
	);
	return 1;
}

int lua_proc_handle_bytescan( lua_State *L ) {
	lua_proc_handle_t *handle = (lua_proc_handle_t*)
		luaL_checkudata(L,1,PROC_HANDLE_CLASS);
	int index = 4;
	uchar *array;
	tscan_t *tscan;
	uintmax_t a;
	uintmax_t from = luaL_optinteger(L,index+1,0);
	uintmax_t upto = luaL_optinteger(L,index+2,INTPTR_MAX);
	_Bool writable = lua_toboolean(L,index+3);
	node_t limit = luaL_optinteger(L,index+4,100), count;
	_Bool all;
	
	if ( limit > LONG_MAX ) limit = 100;
	
	tscan = &(handle->tscan);
	
	count = luaL_optinteger(L,index+5,handle->scan_count);
	all = luaL_optinteger(L,index+6,0);
	if ( all ) {
		count = 0;
		tscan->last_found = 0;
	}
	
	if ( !(array
		= lua_extract_bytes( NULL, L, index, &(handle->bytes) )) )
	{
		lua_pushboolean( L, 0 );
		return 1;
	}
	
	if ( count > tscan->done_scans ||
		!lua_proc_handle_prep_scan(
			handle, count, tscan->last_found, from, upto, writable, ~0 )
	)
	{
		lua_pushboolean( L, 0 );
		return 1;
	}
	
	/* Decide how we will clear memory */
	for ( a = 0; a < tscan->bytes; ++a ) {
		if ( array[a] ) {
			tscan->zero = 0;
			break;
		}
	}
	
	pthread_create(
		&(tscan->thread), NULL, proc_handle_dumper, &(handle->tscan) );

	lua_pushboolean( L, 1 );
	handle->scan_count++;
	return 1;
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
	{ "dump", lua_proc_handle_dump },
	{ "bytescan", lua_proc_handle_bytescan },
	{ "killscan", lua_proc_handle_killscan },
	{ "dumping", lua_proc_handle_dumping },
	{ "scanning", lua_proc_handle_scanning },
	{ "thread_active", lua_proc_handle_thread_active },
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
#define DUMP_CLASS "class_proc_memory"

int lua_proc_memory_term( lua_State *L ) {
	space_t *space = (space_t*)
		luaL_checkudata(L,1,DUMP_CLASS);
	less_space( NULL, space, 0 );
	return 0;
}

int lua_proc_memory_init( lua_State *L ) {
	int want = luaL_checkinteger(L,2);
	space_t *space = (space_t*)
		luaL_checkudata(L,1,DUMP_CLASS);
	if ( space ) lua_proc_memory_term( L );
	lua_pushboolean( L, !!more_space( NULL, space, want ) );
	return 1;
}

int lua_proc_memory_text( lua_State *L ) {
	lua_pushstring( L, DUMP_CLASS );
	return 1;
}
int lua_proc_memory_grab( lua_State *L ) {
	space_t *space = lua_newuserdata(L,sizeof(space_t));
	if ( !space ) return 0;
	space->given = 0;
	space->block = NULL;
	luaL_setmetatable(L,DUMP_CLASS);
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
		luaL_checkudata(L,1,DUMP_CLASS);
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
	luaL_newmetatable(L, DUMP_CLASS);
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
	lua_setglobal(L, DUMP_CLASS );
}
void lua_create_proc_classes( lua_State *L ) {
	lua_proc_create_glance_class(L);
	lua_proc_create_handle_class(L);
}
