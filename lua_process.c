#include "gasp.h"
#define PROC_GLANCE_CLASS "class_pglance"

int lua_proc_process_info( lua_State *L, process_t *process )
{
	if ( !process )
		return 0;
	lua_createtable( L, 0, 5 );
	push_branch_bool( L, "self", process->self );
	push_branch_int( L, "entryId", process->pid );
	push_branch_int( L, "ownerId", process->parent );
	push_branch_str( L, "path", process->path );
	push_branch_str( L, "name", (char*)(process->name.space.block) );
	push_branch_str( L, "cmdl", (char*)(process->cmdl.space.block) );
	return 1;
}

int lua_pglance_init( lua_State *L ) {
	int underId = luaL_optinteger(L,2,0);
	pglance_t *glance = luaL_checkudata(L,1,PROC_GLANCE_CLASS);
	if ( pglance_init( glance, underId ) != 0 )
		return 0;
	return lua_proc_process_info( L, &(glance->process) );
}

int lua_pglance_term( lua_State *L ) {
	pglance_t *glance = luaL_checkudata(L,1,PROC_GLANCE_CLASS);
	pglance_term( glance );
	return 0;
}

int lua_proc_process_next( lua_State *L ) {
	pglance_t *glance = luaL_checkudata(L,1,PROC_GLANCE_CLASS);
	process_next( glance );
	return lua_proc_process_info( L, &(glance->process) );
}

int lua_process_find( lua_State *L ) {
	int ret, underId = luaL_optinteger(L,2,0);
	char const *name = luaL_optstring(L,1,NULL);
	nodes_t nodes = {0};
	process_t *process;
	node_t i;
	
	if ( (ret = process_find( name, &nodes, underId, 0 )) != 0 )
		return 0;
	
	process = nodes.space.block;	
	lua_createtable( L, nodes.count + 1, 0 );
	for ( i = 0; i < nodes.count; ++i, ++process ) {
		lua_pushinteger(L,i+1);
		lua_createtable( L, 0, 5 );
		push_branch_bool( L, "self", process->self );
		push_branch_int( L, "entryId", process->pid );
		push_branch_int( L, "ownerId", process->parent );
		push_branch_str( L, "path", process->path );
		push_branch_str( L, "name", (char*)(process->name.space.block) );
		push_branch_str( L, "cmdl", (char*)(process->cmdl.space.block) );
		lua_settable(L,-3);
		process_term( process );
	}
	free_nodes( &nodes );
	return 1;
}
int lua_pglance_text( lua_State *L ) {
	lua_pushstring( L, PROC_GLANCE_CLASS );
	return 1;
}
int lua_pglance_grab( lua_State *L ) {
	pglance_t *glance =
		(pglance_t*)lua_newuserdata(L,sizeof(pglance_t));
	if ( !glance ) return 0;
	luaL_setmetatable(L,PROC_GLANCE_CLASS);
	return 1;
}

luaL_Reg lua_class_pglance_func_list[] = {
	{ "new", lua_pglance_grab },
	{ "init", lua_pglance_init },
	{ "next", lua_proc_process_next },
	{ "term", lua_pglance_term },
	{ "tostring", lua_pglance_text },
{NULL}};

int lua_pglance_node( lua_State *L ) {
	char const *name = NULL;
	node_t num = 0, tmp;
	int i, len = 0;
	pglance_t *glance = luaL_checkudata(L,1,PROC_GLANCE_CLASS);
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
				lua_class_pglance_func_list[num].name;
				++num
			)
			{
				reg = lua_class_pglance_func_list + num;
				if ( strcmp(name,reg->name) == 0 )
					return reg->func(L);
			}
			return 0;
		}
	}
	else return 0;
	if ( i < 1 ) {
		if ( i == 0 ) {
			lua_pushinteger( L, glance->nodes.count );
			return 1;
		}
		return 0;
	}
	num = i - 1;
	if ( num >= glance->nodes.count )
		return 0;
	tmp = glance->nodes.focus;
	glance->nodes.focus = num;
	i = lua_proc_process_next(L);
	glance->nodes.focus = tmp;
	return i;
}

int lua_pglance_make( lua_State *L ) {
	(void)L;
	return 0;
}

int lua_pglance_free( lua_State *L ) {
	lua_pglance_term(L);
	return 0;
}

int lua_pglance_leng( lua_State *L ) {
	pglance_t *glance = luaL_checkudata(L,1,PROC_GLANCE_CLASS);
	lua_pushinteger(L,glance->nodes.count);
	return 1;
}

luaL_Reg lua_class_pglance_meta_list[] = {
	{ "__gc", lua_pglance_free },
	{ "__len", lua_pglance_leng },
	{ "__index",lua_pglance_node },
	{ "__newindex", lua_pglance_make },
{NULL,NULL}};

void lua_proc_create_glance_class( lua_State *L ) {
	int lib_id, meta_id;

	/* newclass = {} */
	lua_createtable(L, 0, 0);
	lib_id = lua_gettop(L);
	
	/* metatable = {} */
	luaL_newmetatable(L, PROC_GLANCE_CLASS);
	meta_id = lua_gettop(L);
	luaL_setfuncs(L, lua_class_pglance_meta_list, 0);
	
	/* metatable.__index = _methods */
	luaL_newlib( L, lua_class_pglance_func_list );
	lua_setfield( L, meta_id, "__index" );  

	/* metatable.__metatable = _meta */
	luaL_newlib( L, lua_class_pglance_meta_list );
	lua_setfield( L, meta_id, "__metatable");

	/* class.__metatable = metatable */
	lua_setmetatable( L, lib_id );

	/* _G["Foo"] = newclass */
	lua_setglobal(L, PROC_GLANCE_CLASS );
}

#define PROC_HANDLE_CLASS "class_process"

typedef struct lua_process {
	nodes_t bytes;
	tscan_t tscan;
	process_t process;
} lua_process_t;

int lua_process__valid( lua_process_t *lua_process )
{
	if ( !lua_process )
		return EINVAL;
	return process_test(&(lua_process->process));
}

bool lua_process__end_scan( tscan_t *tscan ) {
	int ret = SIGSTOP;
	_Bool success = 1;
	if ( tscan->threadmade ) {
		success = gasp_tpollo( tscan->scan_pipes, SIGKILL, -1 );
		if ( success && tscan->threadmade )
			success = gasp_tpolli( tscan->main_pipes, &ret, -1 );
	}
	return success;
}

bool lua_process__set_rdwr(
	lua_process_t *lua_process, bool want2rdwr
)
{
	int ret = want2rdwr ? SIGSTOP : SIGCONT;
	tscan_t *tscan;
	
	if ( !lua_process )
		return 0;
	
	tscan = &(lua_process->tscan);
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

int lua_process__ret_empty_list( lua_State *L, node_t done, node_t found ) {
	lua_pushinteger(L,done);
	lua_pushinteger(L,found);
	lua_newtable( L );
	return 3;
}

int lua_process__get_scan_list(
	lua_State *L, lua_process_t *lua_process
)
{
	node_t i, count = 0;
	uintmax_t *ptrs;
	tscan_t *tscan;
	nodes_t *nodes;
	
	if ( !L )
		return 0;

	if ( !lua_process )
		return lua_process__ret_empty_list(L,0,0);

	tscan = &(lua_process->tscan);
	nodes = &(tscan->locations);
	ptrs = nodes->space.block;
	
	/* Grab current address and amount found */
	lua_pushinteger( L, lua_process->tscan.done_upto );
	lua_pushinteger( L, (count = nodes->count) );
	
	/* Pre-create the entire table */
	lua_createtable( L, count, 0 );
	
	if ( !count )
		goto done;
	
	for ( i = 0; i < count; ++i ) {
		lua_pushinteger( L, i + 1 );
		lua_pushinteger( L, ptrs[i] );
		lua_settable(L,-3);
	}
	
	done:
	return 3;
}

void lua_process_shutfiles( lua_process_t *lua_process )
{
	int ret = SIGSTOP;
	tscan_t *tscan = &(lua_process->tscan);
	
	if ( tscan->pipesmade ) {
		if ( tscan->threadmade ) {
			(void)gasp_tpollo( tscan->scan_pipes, SIGKILL, -1 );
			(void)gasp_tpolli( tscan->main_pipes, &ret, -1 );
		}
		gasp_close_pipes( tscan->scan_pipes );
		gasp_close_pipes( tscan->main_pipes );
		tscan->pipesmade = 0;
	}
	
	/* sleep call is needed here to ensure system passes execution to
	 * the other thread when it exists on the same CPU core */
	dump_files_shut( lua_process->tscan.dump );
	dump_files_shut( lua_process->tscan.dump + 1 );
}

int lua_process_undo( lua_process_t *lua_process, node_t scan )
{
	char const *GASP_PATH = getenv("GASP_PATH");
	char *path, *pof;
	int ret = 0;
	node_t i;
	space_t path_space = {0}, space = {0};
	dump_t *dump;
	tscan_t *tscan;
	
	pof = "lua process object check";
	if ( !lua_process )
	{
		ret = EINVAL;
		goto fail;
	}
	
	tscan = &(lua_process->tscan);
	dump = tscan->dump + 1;
	
	dump_files_shut( tscan->dump );
	
	if ( scan == tscan->done_scans )
	{
		(void)memmove( tscan->dump, dump, sizeof(dump_t) );
		return dump_files_init(dump);
	}

	dump_files_shut( dump );
	
	pof = "alloc path memory";
	if ( (ret = more_space(
		&path_space, strlen( GASP_PATH ) + UCHAR_MAX)) != 0
	) goto fail;
	path = path_space.block;
	
	if ( !scan ) {
		sprintf( path, "rm -r \"%s/scans/%lu\"",
				GASP_PATH, (ulong)(tscan->id) );
			
		pof = "path is directory check";
		if ( gasp_isdir( path, 0 ) == 0 )
		{
			ret = errno;
			fprintf( stderr, "%s\n", path );
			system( path );
		}
		tscan->id = 0;
		tscan->assignedID = 0;
		goto done;
	}
	
	for ( i = tscan->done_scans - 1; i > scan; --i ) {
		sprintf( path, "rm \"%s/scans/%lu/%lu*\"",
				GASP_PATH, (ulong)(tscan->id), (ulong)i );
		fprintf( stderr, "%s\n", path );
		system( path );
	}
	
	done:
	tscan->done_scans = scan;
	
	if ( ret != 0 )
	{
		fail:
		FAILED( ret, pof );
	}
	
	free_space( &space );
	return ret;
}

int lua_process_term( lua_State *L ) {
	lua_process_t *lua_process = (lua_process_t*)
		luaL_checkudata(L,1,PROC_HANDLE_CLASS);
	tscan_t *tscan = &(lua_process->tscan);
	
	gasp_close_pipes( tscan->main_pipes );
	gasp_close_pipes( tscan->scan_pipes );
	tscan->pipesmade = 0;
	
	(void)lua_process_undo( lua_process, 0 );
	process_term( &(lua_process->process) );
	
	dump_files_shut( tscan->dump );
	dump_files_shut( tscan->dump + 1 );
	
	free_nodes( &(lua_process->bytes) );
	return 0;
}

int lua_process_valid( lua_State *L ) {
	lua_process_t *lua_process = (lua_process_t*)
		luaL_checkudata(L,1,PROC_HANDLE_CLASS);
	lua_pushboolean( L, lua_process__valid( lua_process ) == 0 );
	return 1;
}

int lua_process_init( lua_State *L ) {
	lua_process_t *lua_process = (lua_process_t*)
		luaL_checkudata(L,1,PROC_HANDLE_CLASS);
	int pid = luaL_checkinteger(L,2),
		opt = luaL_optinteger(L,3,PROCESS_O_RWX);
		
	if ( lua_process->process.path[0] )
		lua_process_term( L );
	
	if ( pid > 0 )
		process_info( &(lua_process->process), pid, 1, opt );
	return lua_process_valid(L);
}

int lua_process_glance_data( lua_State *L ) {
	lua_process_t *lua_process = (lua_process_t*)
		luaL_checkudata(L,1,PROC_HANDLE_CLASS);
	uintmax_t addr = luaL_checkinteger(L,2);
	ssize_t i, size = luaL_checkinteger(L,3);
	uchar * array;
	int ret = 0;
	char *pof;
	
	pof = "process check";
	if ( (ret = lua_process__valid( lua_process )) != 0 || size < 1 )
	{
		if ( ret == 0 )
			ret = ERANGE;
		lua_newtable(L);
		goto fail;
	}
	
	array = calloc( size, 1 );
	if ( !array ) {
		lua_newtable(L);
		goto fail;
	}
	
	if ( lua_process__set_rdwr(lua_process,1) )
		ret = pglance_data(
			&(lua_process->process), addr, array, size, &size );
	lua_process__set_rdwr(lua_process,0);
	
	if ( ret != 0 )
	{
		lua_newtable(L);
		goto done;
	}
	
	if ( size < 0 ) size = 0;
	lua_createtable( L, size, 0 );
	for ( i = 0; i < size; ++i )
	{
		lua_pushinteger(L,i+1);
		lua_pushinteger(L,array[i]);
		lua_settable(L,-3);
	}
	
	done:
	free(array);
	if ( ret != 0 )
	{
		fail:
		FAILED( ret, pof );
	}
	lua_pushinteger( L, ret );
	return 2;
}
int lua_process_change_data( lua_State *L ) {
	lua_process_t *lua_process = (lua_process_t*)
		luaL_checkudata(L,1,PROC_HANDLE_CLASS);
	uintmax_t addr = luaL_checkinteger(L,2);
	uchar * array = NULL;
	nodes_t *nodes;
	ssize_t size = 0;
	int ret = 0;
	char *pof;
	
	pof = "process check";
	if ( (ret = lua_process__valid( lua_process )) != 0 )
		goto fail;
	nodes = &(lua_process->bytes);
	
	pof = "bytes extraction";
	if ( (ret = lua_extract_bytes( L, 5, nodes, &array )) != 0 )
		goto fail;
	
	pof = "data change";
	//lua_process__set_rdwr(lua_process,1);
	ret = pchange_data(
		&(lua_process->process), addr, array, nodes->count, &size );
	//lua_process__set_rdwr(lua_process,0);

	if ( ret != 0 )
	{
		fail:
		FAILED( ret, pof );
	}
	lua_pushinteger(L,(ret == 0 && size > 0) ? size : 0);
	lua_pushinteger(L,ret);
	return 2;
}

int lua_process_done_scans( lua_State *L ) {
	lua_process_t *lua_process = (lua_process_t*)
		luaL_checkudata(L,1,PROC_HANDLE_CLASS);
	lua_pushinteger( L, lua_process->tscan.done_scans );
	return 1;
}

int lua_process_scan_done_upto( lua_State *L ) {
	lua_process_t *lua_process = (lua_process_t*)
		luaL_checkudata(L,1,PROC_HANDLE_CLASS);
	lua_pushinteger( L, lua_process->tscan.done_upto );
	return 1;
}
int lua_process_thread_active( lua_State *L ) {
	lua_process_t *lua_process = (lua_process_t*)
		luaL_checkudata(L,1,PROC_HANDLE_CLASS);
	lua_pushboolean( L, lua_process->tscan.threadmade );
	return 1;
}
int lua_process_scanning( lua_State *L ) {
	lua_process_t *lua_process = (lua_process_t*)
		luaL_checkudata(L,1,PROC_HANDLE_CLASS);
	lua_pushboolean( L, lua_process->tscan.scanning );
	return 1;
}
int lua_process_dumping( lua_State *L ) {
	lua_process_t *lua_process = (lua_process_t*)
		luaL_checkudata(L,1,PROC_HANDLE_CLASS);
	lua_pushboolean( L, lua_process->tscan.dumping );
	return 1;
}

int lua_process_scan_found( lua_State *L ) {
	lua_process_t *lua_process = (lua_process_t*)
		luaL_checkudata(L,1,PROC_HANDLE_CLASS);
	lua_pushinteger( L, lua_process->tscan.found );
	return 1;
}

int lua_process_get_scan_list( lua_State *L ) {
	return lua_process__get_scan_list(
		L, (lua_process_t*)luaL_checkudata(L,1,PROC_HANDLE_CLASS) );
}

int lua_process__create_pipes( lua_process_t *lua_process )
{
	int ret = 0;
	char *pof;
	tscan_t *tscan;
	
	pof = "process handle check";
	if ( !lua_process )
	{
		ret = EDESTADDRREQ;
		goto fail;
	}
	
	tscan = &(lua_process->tscan);
	
	if ( tscan->pipesmade )
		return 0;
	
	if ( tscan->main_pipes[0] <= 0 )
	{
		pof = "main pipes creation";
		errno = 0;
		if ( pipe2( tscan->main_pipes, 0 ) == -1 )
		{
			ret = errno;
			goto fail;
		}
	}
	
	if ( tscan->scan_pipes[0] <= 0 )
	{
		pof = "scan pipes creation";
		errno = 0;
		if ( pipe2( tscan->scan_pipes, 0 ) == -1 )
		{
			ret = errno;
			goto fail;
		}
	}
	
	tscan->pipesmade = 1;
	
	if ( ret != 0 )
	{
		fail:
		FAILED( ret, pof );
	}
	
	return ret;
}

int lua_process__can_scan(
	lua_process_t *lua_process
)
{
	int ret = 0;
	char *pof;
	tscan_t *tscan;
	
	pof = "process handle check";
	if ( !lua_process )
	{
		ret = EDESTADDRREQ;
		goto fail;
	}
	
	tscan = &(lua_process->tscan);
	
#ifdef _WIN32
	pof = "WIN32 process handle check";
	if ( !(lua_process->process.handle) )
	{
		ret = EINVAL;
		goto fail;
	}
#endif

	pof = "thread check";
	if ( tscan->threadmade )
	{
		ret = EBUSY;
		goto fail;
	}
	
	pof = "pipe creation";
	if ( (ret = lua_process__create_pipes( lua_process )) != 0 )
		goto fail;
	
	if ( ret != 0 )
	{
		fail:
		FAILED( ret, pof );
	}
	return ret;
}

int lua_process_killscan( lua_State *L ) {
	lua_process_t *lua_process = (lua_process_t*)
		luaL_checkudata(L,1,PROC_HANDLE_CLASS);
	lua_pushboolean( L,
		lua_process__end_scan( &(lua_process->tscan) )
	);
	return 1;
}

int lua_process_percentage( lua_State *L ) {
	lua_process_t *lua_process = (lua_process_t*)
		luaL_checkudata(L,1,PROC_HANDLE_CLASS);
	tscan_t *tscan = &(lua_process->tscan);
	uintmax_t zero = 0;
	uintmax_t addr = tscan->done_upto;
	uintmax_t upto = tscan->dumping ? ~zero : tscan->upto;
	long double num = addr, dem = upto;
		
	lua_pushboolean( L, tscan->threadmade );
	lua_pushboolean( L, tscan->dumping );
	lua_pushboolean( L, tscan->scanning );
	lua_pushinteger( L, tscan->done_upto );
	/* Must be valid number to prevent crashes due to not handling NAN */
	lua_pushnumber( L, upto ? ((num / dem) * 100.0) : 0.0 );
	lua_pushinteger( L, tscan->locations.count );
	return 6;
}

int lua_process_prep_scan(
	lua_process_t *lua_process,
	node_t scan,
	node_t last_found,
	node_t list_limit,
	uintmax_t from,
	uintmax_t upto,
	bool writable,
	int zero
)
{
	int ret = 0;
	tscan_t *tscan;
	nodes_t *nodes;
	dump_t *dump;
	space_t space = {0};
	char *pof;
	
	pof = "can scan check";
	if ( (ret = lua_process__can_scan( lua_process )) != 0 )
		goto fail;
	
	tscan = &(lua_process->tscan);
	nodes = &(tscan->locations);
	dump = tscan->dump + 1;
	tscan->dump->nodes = dump->nodes = &(tscan->mappings);
	
	pof = "address list allocation";
	if ( (ret = more_nodes( uintmax_t, nodes, list_limit )) != 0 )
		goto fail;
	(void)memset( nodes->space.block, 0, nodes->space.given );
	
	pof = "folder creation";
	if ( !(tscan->assignedID) )
	{
		for ( tscan->id = 0; tscan->id < INT_MAX; tscan->id++ )
		{
			if ( (ret = dump_files__dir( &space, tscan->id, 0 )) != 0 )
				break;
		}
		free_space(&space);
		if ( ret == ENOTDIR )
			tscan->assignedID = 1;
		else
			goto fail;
	}
	
	pof = "undo scans";
	if ( (ret = lua_process_undo( lua_process, scan )) != 0 )
		goto fail;
	
	tscan->process = &(lua_process->process);
	tscan->bytes = lua_process->bytes.count;
	tscan->array = tscan->bytes ? lua_process->bytes.space.block : NULL;
	tscan->from = from;
	tscan->upto = upto;
	tscan->writeable = writable;
	tscan->done_upto = 0;
	tscan->done_scans = scan;
	tscan->dumping = 0;
	tscan->scanning = 0;
	tscan->threadmade = 0;
	tscan->found = 0;
	tscan->last_found = last_found;
	tscan->zero = zero ? ~0 : 0;
	
	pof = "open output files";
	if ( (ret = dump_files_open( dump, tscan->id, scan )) != 0 )
		goto fail;
	
	pof = "open previous files (if applicable)";
	if ( scan && dump_files_test(tscan->dump[0]) != 0 ) {
		if ( (ret = dump_files_open(
			tscan->dump, tscan->id, scan )) != 0
		) goto fail;
	}
	
	if ( ret != 0 )
	{
		fail:
		FAILED( ret, pof );
	}
	
	return ret;
}

int lua_process_dump( lua_State *L )
{
	int ret = 0;
	char *pof;
	lua_process_t *lua_process = (lua_process_t*)
		luaL_checkudata(L,1,PROC_HANDLE_CLASS);
	tscan_t *tscan;
	uintmax_t from = luaL_optinteger(L,2,0);
	uintmax_t upto = luaL_optinteger(L,3,INTPTR_MAX);
	_Bool writable = lua_toboolean(L,4);
	tscan =  &(lua_process->tscan);
	
	lua_process->bytes.count = 0;
	
	pof = "prep scan";
	if ( (ret = lua_process_prep_scan(
			lua_process, 0, 0, 0, from, upto, writable, 0 )) != 0
	)
	{
		lua_pushboolean(L,0);
		goto fail;
	}
	
	errno = 0;
	if ( pthread_create(
		&(tscan->thread), NULL, process_dumper, tscan ) != 0
	)
	{
		ret = errno;
		goto fail;
	}
	
	if ( ret != 0 )
	{
		fail:
		FAILED( ret, pof );
	}
	lua_pushboolean( L, (ret == 0) );
	return 1;
}

int lua_process_bytescan( lua_State *L ) {
	lua_process_t *lua_process = (lua_process_t*)
		luaL_checkudata(L,1,PROC_HANDLE_CLASS);
	int ret = 0, index = 4;
	uchar *array;
	char *pof;
	tscan_t *tscan;
	uintmax_t a;
	uintmax_t from = luaL_optinteger(L,index+1,0);
	uintmax_t upto = luaL_optinteger(L,index+2,INTPTR_MAX);
	_Bool writable = lua_toboolean(L,index+3);
	node_t limit = luaL_optinteger(L,index+4,100), scan;
	
	pof = "conflicts check";
	if ( (ret = lua_process__can_scan( lua_process )) != 0 )
		goto fail;
	
	if ( limit > INT_MAX ) limit = 100;
	
	tscan = &(lua_process->tscan);
	
	scan = luaL_optinteger(L,index+5,tscan->done_scans);
	if ( scan != tscan->done_scans )
		tscan->last_found = 0;
	
	pof = "bytes extraction";
	if ( (ret = lua_extract_bytes(
		L, index, &(lua_process->bytes), &array )) != 0
	) goto fail;
	
	pof = "prep scan";
	if ( scan > tscan->done_scans ||
		(ret = lua_process_prep_scan(
			lua_process, scan, tscan->last_found, limit,
			from, upto, writable, ~0 )) != 0
	) goto fail;
	
	/* Decide how we will clear memory */
	for ( a = 0; a < tscan->bytes; ++a ) {
		if ( array[a] ) {
			tscan->zero = 0;
			break;
		}
	}
	
	pof = "thread creation";
	if ( pthread_create(
		&(tscan->thread), NULL, process_dumper, tscan ) != 0
	)
	{
		ret = errno;
		goto fail;
	}

	if ( ret != 0 )
	{
		fail:
		FAILED( ret, pof );
	}
	lua_pushboolean( L, 1 );
	lua_pushinteger( L, ret );
	return 2;
}

int lua_process_text( lua_State *L ) {
	lua_pushstring( L, PROC_HANDLE_CLASS );
	return 1;
}

int lua_process_grab( lua_State *L )
{
	lua_process_t *lua_process =
		(lua_process_t*)lua_newuserdata(L,sizeof(lua_process_t));
	tscan_t *tscan;
	dump_t *dump;
	
	if ( !lua_process )
		return 0;
	
	(void)memset( lua_process, 0, sizeof(lua_process_t) );
	
	tscan = &(lua_process->tscan);
	
	dump = tscan->dump;
	dump->info.fd = dump->used.fd = dump->data.fd = -1;
	
	++dump;
	dump->info.fd = dump->used.fd = dump->data.fd = -1;
	
	tscan->scan_pipes[0] = -1;
	tscan->scan_pipes[1] = -1;
	tscan->main_pipes[0] = -1;
	tscan->main_pipes[1] = -1;
	
	(void)lua_process__create_pipes( lua_process );
	
	luaL_setmetatable(L,PROC_HANDLE_CLASS);
	return 1;
}

luaL_Reg lua_class_process_func_list[] = {
	{ "new", lua_process_grab },
	{ "init", lua_process_init },
	{ "valid", lua_process_valid },
	{ "glance", lua_process_glance_data },
	{ "change", lua_process_change_data },
	{ "dump", lua_process_dump },
	{ "bytescan", lua_process_bytescan },
	{ "killscan", lua_process_killscan },
	{ "dumping", lua_process_dumping },
	{ "scanning", lua_process_scanning },
	{ "percentage_done", lua_process_percentage },
	{ "thread_active", lua_process_thread_active },
	{ "scan_found", lua_process_scan_found },
	{ "scan_done_upto", lua_process_scan_done_upto },
	{ "done_scans", lua_process_done_scans },
	{ "get_scan_list", lua_process_get_scan_list },
	{ "term", lua_process_term },
	{ "tostring", lua_process_text },
{NULL}};

int lua_process_node( lua_State *L ) {
	char const *name = NULL;
	node_t num = 0;
	int i, len = 0;
	luaL_Reg *reg;
	if ( lua_isstring(L,2) ) {
		name = lua_tostring(L,2);
		len = strlen(name);
		if ( sscanf( name, "%d", &i ) < len ) {
			for ( num = 0;
				lua_class_process_func_list[num].name;
				++num
			)
			{
				reg = lua_class_process_func_list + num;
				if ( strcmp(name,reg->name) == 0 )
					return reg->func(L);
			}
			return 0;
		}
	}
	return 0;
}

int lua_process_make( lua_State *L ) {
	(void)L;
	return 0;
}

int lua_process_free( lua_State *L ) {
	lua_process_term(L);
	return 0;
}

luaL_Reg lua_class_process_meta_list[] = {
	{ "__gc", lua_process_free },
	{ "__index",lua_process_node },
	{ "__newindex", lua_process_make },
{NULL,NULL}};

void lua_proc_create_handle_class( lua_State *L ) {
	int lib_id, meta_id;

	/* newclass = {} */
	lua_createtable(L, 0, 0);
	lib_id = lua_gettop(L);
	
	/* metatable = {} */
	luaL_newmetatable(L, PROC_HANDLE_CLASS);
	meta_id = lua_gettop(L);
	luaL_setfuncs(L, lua_class_process_meta_list, 0);
	
	/* metatable.__index = _methods */
	luaL_newlib( L, lua_class_process_func_list );
	lua_setfield( L, meta_id, "__index" );  

	/* metatable.__metatable = _meta */
	luaL_newlib( L, lua_class_process_meta_list );
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
	free_space( space );
	return 0;
}

int lua_proc_memory_init( lua_State *L ) {
	int want = luaL_checkinteger(L,2);
	space_t *space = (space_t*)
		luaL_checkudata(L,1,DUMP_CLASS);
	if ( space ) lua_proc_memory_term( L );
	lua_pushboolean( L, more_space( space, want ) == 0 );
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
