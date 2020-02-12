#include "gasp.h"

mode_t file_glance_perm( int *err, char const *path) {
	struct stat st;
	if ( stat( path, &st ) == 0 )
		return st.st_mode;
	if ( err ) *err = errno;
	return 0;
}

mode_t file_change_perm( int *err, char const * path, mode_t set ) {
	int ret = EXIT_SUCCESS;
	mode_t was = file_glance_perm( &ret, path );
	if ( !was && ret != EXIT_SUCCESS ) {
		if ( err ) *err = ret;
		return 0;
	}
	if ( chmod( path, set ) != 0 ) {
		if ( err ) *err = errno;
		return was;
	}
	return was;
}

size_t file_glance_size( int *err, char const *path )
{
	int fd;
	char buff[BUFSIZ];
	gasp_off_t size = 0, tmp = -1;
	struct stat st;
	
	errno = EXIT_SUCCESS;
	
	if ( stat( path, &st ) == 0 && st.st_size > 0 )
		return st.st_size;
	
	if ( access( path, F_OK ) != 0 ) {
		if ( err ) *err = errno;
		ERRMSG( errno, "Invalid path" );
		fprintf( stderr, "Path: '%s'\n", path );
		return 0;
	}
	
	errno = EXIT_SUCCESS;
	if ( (fd = open(path,O_RDONLY)) < 0 ) {
		if ( err ) *err = errno;
		ERRMSG( errno, "Don't have read access path" );
		return 0;
	}
	
	while ( (tmp = read( fd, buff, BUFSIZ )) > 0 )
		size += tmp;
	
	close(fd);
	
	if ( size <= 0 ) {
		if ( err ) *err = errno;
		return size;
	}
	
	return size;
}
void* proc_handle_wait( void *data ) {
	proc_handle_t *handle = data;
	int status = 0;
	(void)pthread_detach( handle->thread );
	
	while ( handle->waiting ) {
		waitpid(handle->notice.entryId,&status,0);
		if ( WIFEXITED(status) || WIFSIGNALED(status) ) {
			handle->running = 0;
			break;
		}
	}
	
	handle->waiting = 0;
	return data;
}
void* proc_handle_catch_sigstop_on_attach( void *data ) {
	int status = 0;
	proc_handle_t *handle = data;
	handle->running = 1;
	while ( 1 ) {
		waitpid( handle->notice.entryId, &status, 0 );
		if ( WIFEXITED(status) || WIFSIGNALED(status) ) {
			handle->running = 0;
			break;
		}
	}
	return data;
}
proc_handle_t* proc_handle_open( int *err, int pid )
{
	proc_handle_t *handle;
	
	if ( pid <= 0 ) {
		errno = EINVAL;
		if ( err ) *err = errno;
		ERRMSG( errno, "Invalid Pid");
		return NULL;
	}

	if ( !(handle = calloc(sizeof(proc_handle_t),1)) ) {
		if ( err ) *err = errno;
		ERRMSG( errno, "Couldn't allocate memory for handle");
		return NULL;
	}
	
	if ( !proc_notice_info( err, pid, &(handle->notice) ) ) {
		proc_handle_shut( handle );
		return NULL;
	}
	
	if ( handle->notice.self )
		sprintf( handle->procdir, "/proc/self" );
	else
		sprintf( handle->procdir, "/proc/%d", pid );
	
	handle->running = 1;
	
	return handle;
}

void proc_handle_shut( proc_handle_t *handle ) {
	/* Used only to prevent segfault from pthread_join, do nothing
	 * with it */
	void *data;
	
	/* Don't try to close NULL as that will segfault */
	if ( !handle ) return;
	
	/* Ensure thread closes before we release the handle it's using */
	if ( handle->waiting ) {
		handle->waiting = 0;
		(void)pthread_join( handle->thread, &data );
	}
	
	/* Cleanup noticed info */
	proc_notice_zero( &(handle->notice) );
	
	/* Ensure that even if handle is reused by accident the most that
	 * can happen is a segfault */
	(void)memset(handle,0,sizeof(proc_handle_t));
	
	/* No need for the handle anymore */
	free(handle);
}

#define PAGE_LINE_SIZE ((sizeof(void*) * 4) + 7)

intptr_t proc_mapped_next(
	int *err, proc_handle_t *handle,
	intptr_t addr, proc_mapped_t *mapped, mode_t prot
)
{
	int fd;
	char line[PAGE_LINE_SIZE] = {0}, *pos, path[256] = {0};
	
	errno = EXIT_SUCCESS;
	if ( !mapped ) {
		if ( err ) *err = EDESTADDRREQ;
		ERRMSG( EINVAL, "Need destination for page details" );
		return -1;
	}
	
	memset( mapped, 0, sizeof(proc_mapped_t) );
	if ( !handle ) {
		if ( err ) *err = EINVAL;
		ERRMSG( EINVAL, "Invalid handle" );
		return -1;
	}
	
	sprintf( path, "%s/maps", handle->procdir );
	if ( (fd = open( path, O_RDONLY )) < 0 ) {
		if ( err ) *err = errno;
		ERRMSG( errno, "Failed to open maps file" );
		fprintf( stderr, "Path: '%s'\n", path );
		return -1;
	}
	
	do {
		memset( mapped, 0, sizeof(proc_mapped_t) );
		do {
			memset( line, 0, PAGE_LINE_SIZE );
			errno = EXIT_SUCCESS;
			if ( gasp_read( fd, line, PAGE_LINE_SIZE )
				!= PAGE_LINE_SIZE )
				goto failed;
			
			line[PAGE_LINE_SIZE-1] = 0;
			sscanf( line, "%p-%p",
				(void**)&(mapped->base),
				(void**)&(mapped->upto) );

			if ( addr < mapped->upto )
				break;
			
			/* Move file pointer to next line */
			while ( gasp_read( fd, line, 1 ) == 1 ) {
				if ( line[0] == '\n' )
					break;
				line[0] = 0;
			}
			
			/* Ensure we break out when we hit EOF */
			if ( line[0] != '\n' )
				goto failed;
		} while ( addr >= mapped->upto );
		
		/* Get position right after address range */
		pos = strchr( line, ' ' );
		/* Capture actual permissions */
		*pos = 0;
		sprintf( path, "%s/map_files/%s", handle->procdir, line );
		mapped->perm = file_glance_perm( err, path );
		/* Move pointer to the permissions text */
		++pos;
		
		mapped->size = (mapped->upto) - (mapped->base);
		mapped->prot |= (pos[0] == 'r') ? 04 : 0;
		mapped->prot |= (pos[1] == 'w') ? 02 : 0;
		mapped->prot |= (pos[2] == 'x') ? 01 : 0;
		/* Check if shared */
		mapped->prot |= (pos[3] == 'p') ? 0 : 010;
		if ( mapped->prot & 010 )
			/* Check if validated share */
			mapped->prot |= (pos[3] == 's') ? 0 : 020;
		
		addr = mapped->upto;
	} while ( (mapped->prot & prot) != prot );
	
	close(fd);
	return mapped->base;
	
	failed:
	close(fd);
	if ( err ) *err = errno;
	if ( errno == EXIT_SUCCESS )
		return -1;
	ERRMSG( errno, "Failed to find page that finishes after address" );
	fprintf( stderr, "Wanted: From address %p a page with %c%c%c%c\n",
		(void*)addr,
		((prot & 04) ? 'r' : '-'),
		((prot & 02) ? 'w' : '-'),
		((prot & 01) ? 'x' : '-'),
		((prot & 010) ? ((prot & 020) ? 'v' : 's') : '-')
	);
	return -1;
}

mode_t proc_mapped_addr(
	int *err, proc_handle_t *handle,
	intptr_t addr, mode_t perm, int *fd
)
{
	char path[256] = {0};
	proc_mapped_t mapped = {0};
	
	while ( proc_mapped_next( err, handle, mapped.upto, &mapped, 0 )
		>= 0 ) {
		if ( mapped.upto < addr ) continue;
#ifdef SIZEOF_LLONG
		sprintf( path, "%s/map_files/%llx-%llx",
			handle->procdir,
			(long long)(mapped.base), (long long)(mapped.upto) );
#else
		sprintf( path, "%s/map_files/%lx-%lx",
			handle->procdir,
			(long)(mapped.base), (long)(mapped.upto) );
#endif
		if ( access( path, F_OK ) == 0 ) {
			(void)file_change_perm( err, path, perm | mapped.perm );
			if ( fd ) *fd = open( path, O_RDWR );
			return mapped.perm;
		}
	}
	
	if ( err ) *err = ERANGE;
	return 0;
}

int proc__rwvmem_test(
	proc_handle_t *handle, void *mem, size_t size
)
{
	int ret = EXIT_SUCCESS;
	
	if ( !handle || !mem || !size ) {
		ret = EINVAL;
		if ( !handle ) ERRMSG( ret, "Invalid handle" );
		if ( !mem ) ERRMSG( ret, "Invalid memory pointer" );
		if ( !size ) ERRMSG( ret, "Invalid memory size" );
		return 0;
	}
	
	return ret;
}

node_t proc_aobscan(
	int *err, int into,
	proc_handle_t *handle,
	uchar *array, intptr_t bytes,
	intptr_t from, intptr_t upto,
	bool writable
)
{
	int ret = EXIT_SUCCESS;
	node_t count = 0;
	uchar buff[BUFSIZ*2] = {0}, *i, *next;
	intptr_t done;
	proc_mapped_t mapped = {0};
	_Bool load_next = 0;
	mode_t prot = 04 | (writable ? 02 : 0);
	
	errno = EXIT_SUCCESS;
	
	if ( (ret = proc__rwvmem_test( handle, array, bytes ))
		!= EXIT_SUCCESS ) {
		if ( err ) *err = ret;
		return 0;
	}
	
	if ( into < 0 ) {
		ret = EDESTADDRREQ;
		if ( err ) *err = ret;
		ERRMSG( ret, "into needs a file descriptor" );
		return 0;
	}
	
	if ( gasp_lseek( into, 0, SEEK_SET ) < 0 ) {
		if ( err ) *err = errno;
		ERRMSG( errno, "failed reset position for output list file" );
		return 0;
	}
	
	if ( proc_mapped_next( &ret, handle, from, &mapped, prot ) < 0 ) {
		if ( err ) *err = ret;
		ERRMSG( ret, "no pages with base address >= start address" );
		return 0;
	}
	
	if ( mapped.base >= (upto - bytes) ) {
		if ( err ) *err = EXIT_SUCCESS;
		return 0;
	}
	if ( from < mapped.base ) from = mapped.base;
	
	done = mapped.upto - from;
	if ( done >= BUFSIZ ) done = BUFSIZ;
	if ( (done = proc_glance_data( &ret, handle, from, buff, done ))
		<= 0 ) {
		if ( err ) *err = ret;
		ERRMSG( ret, "failed to read from process memory" );
		return 0;
	}
	
	while ( (from + bytes) < upto ) {
		done = mapped.upto - from;	
		if ( done >= BUFSIZ ) {
			if ( done > BUFSIZ ) done = BUFSIZ;		
			if ( (done =
				proc_glance_data( err, handle, from,
				buff + BUFSIZ, done )) < 0) {
				ERRMSG( errno, "failed read from process memory" );
				fprintf( stderr, "Read %ld in Range %p-%p %c%c%c%c\n",
					(long)done, (void*)from, (void*)(mapped.upto),
					(mapped.perm & 04) ? 'r' : '-',
					(mapped.perm & 02) ? 'w' : '-',
					(mapped.perm & 01) ? 'x' : '-',
					(mapped.perm & 010) ? 'p' : 's' );
				return count;
			}
			next = buff + BUFSIZ;
		}
		else {
			next = buff + (done - bytes) + 1;
			load_next = 1;
		}
		
		for ( i = buff; i < next; ++i, ++from ) {
			if ( memcmp( i, array, bytes ) == 0 ) {
				if ( write( into, &from, sizeof(from) ) != sizeof(from) ) {
					if ( err ) *err = errno;
					ERRMSG( errno, "failed to copy found address" );
					return count;
				}
				++count;
			}
		}
		
		(void)memmove( buff, buff + BUFSIZ, BUFSIZ );
		(void)memset( buff + BUFSIZ, 0, BUFSIZ );
		
		if ( load_next ) {
			if ( proc_mapped_next( &ret, handle,
				mapped.upto, &mapped, prot ) < 0 ) {
				if ( err ) *err = ret;
				if ( ret != EXIT_SUCCESS )
					ERRMSG( ret, "Couldn't read VM" );
				return count;
			}
			
			if ( mapped.base >= (upto - bytes) ) {
				if ( err ) *err = EXIT_SUCCESS;
				return count;
			}
			
			if ( from < mapped.base ) from = mapped.base;
		}
	}
	
	if ( err ) *err = EXIT_SUCCESS;
	return count;
}

proc_notice_t* proc_locate_name(
	int *err, char const *name, nodes_t *nodes, int underId )
{
	int ret = EXIT_SUCCESS;
	node_t i = 0;
	size_t size;
	proc_glance_t glance;
	proc_notice_t *notice, *noticed;
	char *text;
	
	if ( !nodes ) {
		ret = EDESTADDRREQ;
		if ( err ) *err = ret;
		ERRMSG( ret, "nowhere to place noticed processes" );
		return NULL;
	}

	if ( err ) *err = ret;
	memset( nodes, 0, sizeof(nodes_t) );
	
	for ( notice = proc_glance_init( &ret, &glance, underId )
		; notice; notice = proc_notice_next( &ret, &glance )
	)
	{
		text = notice->name.block;
		if ( !strstr( text, name ) ) {
			text = notice->cmdl.block;
			if ( !strstr( text, name ) )
				continue;
		}
			
		if ( (ret = add_node( nodes, &i, sizeof(proc_notice_t) ))
			!= EXIT_SUCCESS )
			break;
		
		text = notice->name.block;
		
		noticed = (proc_notice_t*)(nodes->space.block);
		noticed[i].entryId = notice->entryId;
		if ( !more_space(
			&ret,&(noticed[i].name), (size = strlen(text)+1)) ) {
			nodes->count--;
			break;
		}
		
		(void)memcpy( noticed[i].name.block, text, size );
		
		text = notice->cmdl.block;
		
		if ( !more_space(
			&ret,&(noticed[i].cmdl), (size = strlen(text)+1)) ) {
			nodes->count--;
			break;
		}
		
		(void)memcpy( noticed[i].cmdl.block, text, size );
	}
	
	proc_glance_term( &glance );
	if ( i > 0 )
		return nodes->space.block;
	if ( ret == EXIT_SUCCESS ) ret = ENOENT;
	if ( err ) *err = ret;
	return NULL;
}

void proc_notice_zero( proc_notice_t *notice ) {
	if ( !notice ) return;
	(void)change_kvpair( &(notice->kvpair), 0, 0, 0, 0 );
	(void)less_space( NULL, &(notice->name), 0 );
	(void)less_space( NULL, &(notice->cmdl), 0 );
}

void proc_glance_term( proc_glance_t *glance ) {
	if ( !glance ) return;
	(void)free_nodes( int, NULL, &(glance->idNodes) );
	proc_notice_zero( &(glance->notice) );
	(void)memset( glance, 0, sizeof(proc_glance_t) );
}

void proc_notice_puts( proc_notice_t *notice ) {
	if ( !notice )
		return;
	fprintf( stderr, "%s() Process %04X under %04X is named '%s'\n",
		__func__, notice->entryId, notice->ownerId,
		(char*)(notice->name.block) );
}

proc_notice_t* proc_notice_info(
	int *err, int pid, proc_notice_t *notice )
{
	int fd;
	char dir[sizeof(int) * CHAR_BIT] = {0};
	char path[256] = {0};
	space_t *name, *full, *cmdl;
	int ret;
	char *k, *v, *n;
	intptr_t size;
	
	if ( !notice ) {
		if ( err ) *err = EDESTADDRREQ;
		ERRMSG( EDESTADDRREQ, "notice was NULL" );
		return NULL;
	}
	name = &(notice->name);
	cmdl = &(notice->cmdl);
	full = &(notice->kvpair.full);
	
	notice->self = (pid == getpid());
	
	if ( notice->self )
		sprintf( dir, "/proc/self" );
	else
		sprintf( dir, "/proc/%d", pid );
	
	memset( name->block, 0, name->given );
	memset( cmdl->block, 0, cmdl->given );
	memset( full->block, 0, full->given );
	
	notice->ownerId = -1;
	notice->entryId = pid;
	
	if ( pid < 1 ) {
		ret = EDESTADDRREQ;
		if ( err ) *err = ret;
		ERRMSG( ret, "pid must not be less than 1");
		proc_notice_puts(notice);
		return NULL;
	}
	
	sprintf( path, "%s/cmdline", dir );
	
	if ( !(size = file_glance_size( &ret, path )) ) {
		if ( err ) *err = ret;
		return NULL;
	}
	
	if ( (fd = open(path,O_RDONLY)) < 0 ) {
		if ( err ) *err = errno;
		ERRMSG( errno, "Couldn't open status file" );
		proc_notice_puts(notice);
		fprintf( stderr, "File '%s'\n", path );
		return NULL;
	}
	
	if ( !more_space( &ret, cmdl, size )) {
		if ( err ) *err = ret;
		ERRMSG( ret, "Couldn't allocate memory to read into" );
		proc_notice_puts(notice);
		fprintf( stderr, "Wanted %zu bytes\n", size );
		close(fd);
		return NULL;
	}
	
	if ( gasp_read(fd, cmdl->block, cmdl->given) < size ) {
		close(fd);
		ret = ENODATA;
		if ( err ) *err = ret;
		ERRMSG( ret, "Couldn't read the status file" );
		proc_notice_puts(notice);
		return NULL;
	}
	/* No longer need the file opened since we have everything */
	close(fd);
	
	sprintf( path, "%s/status", dir );
	
	if ( !(size = file_glance_size( &ret, path )) ) {
		if ( err ) *err = ret;
		return NULL;
	}
	
	if ( (fd = open(path,O_RDONLY)) < 0 ) {
		if ( err ) *err = errno;
		ERRMSG( errno, "Couldn't open status file" );
		proc_notice_puts(notice);
		fprintf( stderr, "File '%s'\n", path );
		return NULL;
	}
	
	if ( !more_space( &ret, full, size )) {
		if ( err ) *err = ret;
		ERRMSG( ret, "Couldn't allocate memory to read into" );
		proc_notice_puts(notice);
		fprintf( stderr, "Wanted %zu bytes\n", size );
		close(fd);
		return NULL;
	}
	
	if ( gasp_read(fd, full->block, full->given) <= 0 ) {
		close(fd);
		ret = ENODATA;
		if ( err ) *err = ret;
		ERRMSG( ret, "Couldn't read the status file" );
		proc_notice_puts(notice);
		return NULL;
	}
	/* No longer need the file opened since we have everything */
	close(fd);
	
	/* Ensure we have enough space for the name */
	if ( !more_space( &ret, name, full->given ) ) {
		if ( err ) *err = ret;
		ERRMSG( ret, "Couldn't allocate memory for name");
		proc_notice_puts(notice);
		return NULL;
	}
	
	n = k = full->block;
	while ( size > 0 ) {
		n = strchr( n, '\n' );
		*n = 0;
		size -= strlen(k) + 1;
		v = strchr( k, ':' );
		*v = 0;
		/* We only care about the name and ppid at the moment */
		if ( strcmp( k, "Name" ) == 0 )
			memcpy( name->block, v + 2, strlen(v+1) );
		if ( strcmp( k, "PPid" ) == 0 )
			notice->ownerId = strtol( v + 2, NULL, 10 );
		*v = ':';
		*n = '\n';
		k = ++n;
	}
	
	if ( *((char*)(notice->name.block)) && notice->ownerId >= 0 ) {
		if ( err ) *err = EXIT_SUCCESS;
		return notice;
	}
	ret = ENOENT;
	if ( err ) *err = ret;
	ERRMSG( ret, "Couldn't locate process" );
	proc_notice_puts(notice);
	return NULL;
}

proc_notice_t*
	proc_notice_next( int *err, proc_glance_t *glance ) {
	proc_notice_t *notice;
	int entryId = 0;
	node_t i;
	
	if ( !glance ) {
		if ( err ) *err = EDESTADDRREQ;
		ERRMSG( EDESTADDRREQ, "glance was NULL" );
		return NULL;
	}
	

	if ( err && *err != EXIT_SUCCESS ) {
		proc_glance_term( glance );
		return NULL;
	}
	
	if ( !(glance->idNodes.count) ) {
		if ( err ) *err = ENODATA;
		ERRMSG( ENODATA, "glance->idNodes.count was 0" );
		proc_glance_term( glance );
		return NULL;
	}

	notice = &(glance->notice);
	for ( i = glance->process; i < glance->idNodes.count; ++i )
	{
		entryId = notice->ownerId =
			((int*)(glance->idNodes.space.block))[i];
		while ( proc_notice_info( err, notice->ownerId, notice ) )
		{
			if ( notice->ownerId == glance->underId ) {
				glance->process = ++i;
				return proc_notice_info( err, entryId, notice );
			}
			if ( notice->ownerId <= 0 )
				break;
		}
	}

	glance->process = glance->idNodes.count;
	if ( err ) *err = EXIT_SUCCESS;
	proc_glance_term( glance );
	return NULL;
}

proc_notice_t*
	proc_glance_init( int *err, proc_glance_t *glance, int underId ) {
	int ret = errno = EXIT_SUCCESS;
	DIR *dir;
	struct dirent *ent;
	node_t i = 0;
	char *name;
	if ( !glance ) {
		if ( err ) *err = EDESTADDRREQ;
		ERRMSG( EDESTADDRREQ, "glance was NULL" );
		return NULL;
	}
	(void)memset( glance, 0, sizeof(proc_glance_t) );
	glance->underId = underId;
	if ( more_nodes( int, &ret, &(glance->idNodes), 2000 ) ) {
		if ( (dir = opendir("/proc")) ) {
			
			while ( (ent = readdir( dir )) ) {
				for ( name = ent->d_name;; ++name ) {
					if ( *name < '0' || *name > '9' )
						break;
				}
				if ( *name )
					continue;
				
				if ( (ret = add_node(
					&(glance->idNodes), &i, sizeof(int) ))
					!= EXIT_SUCCESS )
					break;

				sscanf( ent->d_name, "%d",
					((int*)(glance->idNodes.space.block)) + i
				);
			}
			closedir(dir);
			if ( glance->idNodes.count )
				return proc_notice_next( err, glance );
		}
	}
	ret = ( errno != EXIT_SUCCESS ) ? errno : EXIT_FAILURE;
	if ( err ) *err = ret;
	ERRMSG( ret, "" );
	return NULL;
}

size_t proc__glance_vmem(
	int *err, proc_handle_t *handle,
	intptr_t addr, void *mem, intptr_t size ) {
#ifdef _GNU_SOURCE
	struct iovec internal, external;
	intptr_t done;
	int ret;
	
	if ( (ret = proc__rwvmem_test(handle, mem,size)) != EXIT_SUCCESS )
	{
		if ( err ) *err = ret;
		return 0;
	}

	errno = EXIT_SUCCESS;
	
	internal.iov_base = mem;
	internal.iov_len = size;
	external.iov_base = (void*)addr;
	external.iov_len = size;
	
	if ( (done = process_vm_readv(
		handle->notice.entryId, &internal, 1, &external, 1, 0))
		!= size )
	{
		if ( err ) *err = errno;
		ERRMSG( errno, "Couldn't write to VM" );
		fprintf( stderr, "%p", external.iov_base );
	}
	
	return done;
#else
	if ( err ) *err = ENOSYS;
	return 0;
#endif
}

intptr_t proc__glance_file(
	int *err, proc_handle_t *handle,
	intptr_t addr, void *mem, size_t size
)
{
#ifdef gasp_pread
	char path[SIZEOF_PROCDIR+5] = {0};
	intptr_t done = 0;
	int ret, fd;
	
	if ( (ret = proc__rwvmem_test(handle, mem,size)) != EXIT_SUCCESS )
	{
		if ( err ) *err = ret;
		return 0;
	}
	
	sprintf( path, "%s/mem", handle->procdir );
	if ( (fd = open( path, O_RDONLY )) < 0 ) {
		if ( err ) *err = errno;
		ERRMSG( errno, "Could'nt open process memory" );
		fprintf( stderr, "Path: '%s'\n", path );
		return 0;
	}
	
	done = gasp_pread( fd, mem, size, addr );
	close(fd);
	if ( done < 0 ) {
		if ( err ) *err = errno;
		ERRMSG( errno, "Couldn't read process memory" );
	}
	return done;
#else
	if ( err ) err = ENOSYS;
	return 0;
#endif
}

intptr_t proc__glance_self(
	int *err, proc_handle_t *handle,
	intptr_t addr, void *mem, size_t size
)
{
	int ret;
	
	if ( (ret = proc__rwvmem_test(handle, mem,size)) != EXIT_SUCCESS )
	{
		if ( err ) *err = ret;
		return 0;
	}
	
	if ( handle->notice.self ) {
		(void)memmove( mem, (void*)addr, size );
		if ( err ) *err = errno;
		if ( errno != EXIT_SUCCESS )
			ERRMSG( errno, "Couldn't override VM" );
		return size;
	}
	
	return 0;
}

intptr_t proc__glance_seek(
	int *err, proc_handle_t *handle,
	intptr_t addr, void *mem, size_t size
)
{
	char path[SIZEOF_PROCDIR+5] = {0};
	intptr_t done = 0;
	int ret, fd;
	
	if ( (ret = proc__rwvmem_test(handle, mem,size)) != EXIT_SUCCESS )
	{
		if ( err ) *err = ret;
		return 0;
	}
	
	sprintf( path, "%s/mem", handle->procdir );
	if ( (fd = open( path, O_RDONLY )) < 0 ) {
		if ( err ) *err = errno;
		ERRMSG( errno, "Could'nt open process memory" );
		fprintf( stderr, "Path: '%s'\n", path );
		return 0;
	}
	
	gasp_lseek( fd, addr, SEEK_SET );
	if ( errno != EXIT_SUCCESS )
		goto failed;
	
	done = gasp_read( fd, mem, size );
	if ( done <= 0 || errno != EXIT_SUCCESS )
		goto failed;

	close(fd);
	return done;
	
	failed:
	close(fd);
	if ( err ) *err = errno;
	ERRMSG( errno, "Couldn't read process memory" );
	return 0;
}

intptr_t proc_glance_data(
	int *err, proc_handle_t *handle,
	intptr_t addr, void *mem, size_t size
)
{
	intptr_t done;
	mode_t perm;
	int ret = proc__rwvmem_test( handle, mem, size ), fd = -1;
	
	if ( ret != EXIT_SUCCESS ) {
		if ( err ) *err = ret;
		return 0;
	}
	errno = EXIT_SUCCESS;
	
	ptrace( PTRACE_ATTACH, handle->notice.entryId, NULL, NULL );
	
	if ( (perm = proc_mapped_addr( &ret, handle, addr, 0777, NULL )) == 0
		&& ret != EXIT_SUCCESS ) {
		if ( fd >= 0 ) close(fd);
		if ( err ) *err = ret;
		return 0;
	}
	
	if ( fd >= 0 ) {
		gasp_lseek( fd, addr, SEEK_SET );
		done = gasp_read( fd, mem, size );
		if ( err ) *err = errno;
		close(fd);
		if ( errno )
			ERRMSG( errno, "Couldn't read VM" );
		return done;
	}
	
	if ( (done = proc__glance_vmem( err, handle, addr, mem, size ))
		> 0 ) goto success;
	if ( (done = proc__glance_file( err, handle, addr, mem, size ))
		> 0 ) goto success;
	if ( (done = proc__glance_self( err, handle, addr, mem, size ))
		> 0 ) goto success;
	if ( (done = proc__glance_seek( err, handle, addr, mem, size ))
		> 0 ) goto success;
	
	ptrace( PTRACE_DETACH, handle->notice.entryId, NULL, NULL );
	if ( proc_mapped_addr( &ret, handle, addr, perm, NULL ) == 0
		&& ret != EXIT_SUCCESS && err )
		*err = ret;
	return 0;
	success:
	ptrace( PTRACE_DETACH, handle->notice.entryId, NULL, NULL );
	if ( proc_mapped_addr( &ret, handle, addr, perm, NULL ) == 0
		&& ret != EXIT_SUCCESS && err )
		*err = ret;
	return done;
}

size_t proc__change_vmem(
	int *err, proc_handle_t *handle,
	intptr_t addr, void *mem, intptr_t size ) {
#ifdef _GNU_SOURCE
	struct iovec internal, external;
	intptr_t done;
	int ret;
	
	if ( (ret = proc__rwvmem_test(handle, mem,size)) != EXIT_SUCCESS )
	{
		if ( err ) *err = ret;
		return 0;
	}

	errno = EXIT_SUCCESS;
	
	internal.iov_base = mem;
	internal.iov_len = size;
	external.iov_base = (void*)addr;
	external.iov_len = size;
	
	if ( (done = process_vm_writev(
		handle->notice.entryId, &internal, 1, &external, 1, 0))
		!= size )
	{
		if ( err ) *err = errno;
		ERRMSG( errno, "Couldn't write to VM" );
		fprintf( stderr, "Address %p\n", external.iov_base );
	}
	
	return done;
#else
	if ( err ) *err = ENOSYS;
	return 0;
#endif
}

intptr_t proc__change_file(
	int *err, proc_handle_t *handle,
	intptr_t addr, void *mem, size_t size
)
{
#ifdef gasp_pwrite
	char path[SIZEOF_PROCDIR+5] = {0};
	intptr_t done = 0;
	int ret, fd;
	
	if ( (ret = proc__rwvmem_test(handle, mem,size)) != EXIT_SUCCESS )
	{
		if ( err ) *err = ret;
		return 0;
	}
	
	sprintf( path, "%s/mem", handle->procdir );
	if ( (fd = open( path, O_WRONLY )) < 0 ) {
		if ( err ) *err = errno;
		ERRMSG( errno, "Could'nt open process memory" );
		fprintf( stderr, "Path: '%s'\n", path );
		return 0;
	}
	
	done = gasp_pwrite( fd, mem, size, addr );
	close(fd);
	if ( done < 0 ) {
		if ( err ) *err = errno;
		ERRMSG( errno, "Couldn't write to process memory" );
	}
	return done;
#else
	if ( err ) err = ENOSYS;
	return 0;
#endif
}

size_t proc__change_self(
	int *err, proc_handle_t *handle,
	intptr_t addr, void *mem, size_t size
)
{
	int ret;
	
	if ( (ret = proc__rwvmem_test(handle, mem,size)) != EXIT_SUCCESS )
	{
		if ( err ) *err = ret;
		return 0;
	}
	
	if ( handle->notice.self ) {
		(void)memmove( (void*)addr, mem, size );
		if ( err ) *err = errno;
		if ( errno != EXIT_SUCCESS )
			ERRMSG( errno, "Couldn't override VM" );
		return ret;
	}
	
	return ENOSYS;
}

intptr_t proc__change_seek(
	int *err, proc_handle_t *handle,
	intptr_t addr, void *mem, size_t size
)
{
	char path[SIZEOF_PROCDIR+5] = {0};
	intptr_t done = 0;
	int ret, fd;
	
	if ( (ret = proc__rwvmem_test(handle, mem,size)) != EXIT_SUCCESS )
	{
		if ( err ) *err = ret;
		return 0;
	}
	
	sprintf( path, "%s/mem", handle->procdir );
	if ( (fd = open( path, O_WRONLY )) < 0 ) {
		if ( err ) *err = errno;
		ERRMSG( errno, "Could'nt open process memory" );
		fprintf( stderr, "Path: '%s'\n", path );
		return 0;
	}
	
	gasp_lseek( fd, addr, SEEK_SET );
	if ( errno != EXIT_SUCCESS )
		goto failed;
	
	done = gasp_write( fd, mem, size );
	if ( done <= 0 || errno != EXIT_SUCCESS )
		goto failed;

	close(fd);
	return done;
	
	failed:
	close(fd);
	if ( err ) *err = errno;
	ERRMSG( errno, "Couldn't write to process memory" );
	return 0;
}

intptr_t proc_change_data(
	int *err, proc_handle_t *handle,
	intptr_t addr, void *mem, size_t size
)
{
	intptr_t done;
	mode_t perm;
	int ret = proc__rwvmem_test(handle,mem,size), fd = -1;

	if ( ret != EXIT_SUCCESS ) {
		if ( err ) *err = ret;
		return 0;
	}
	errno = EXIT_SUCCESS;
	
	ptrace( PTRACE_ATTACH, handle->notice.entryId, NULL, NULL );
	
	if ( (perm = proc_mapped_addr( &ret, handle, addr, 0777, NULL ))
		== 0 && ret != EXIT_SUCCESS ) {
		if ( fd >= 0 ) close(fd);
		if ( err ) *err = ret;
		return 0;
	}
	if ( fd >= 0 ) {
		gasp_lseek( fd, addr, SEEK_SET );
		done = gasp_write( fd, mem, size );
		if ( err ) *err = errno;
		close(fd);
		if ( errno )
			ERRMSG( errno, "Couldn't write VM" );
		return done;
	}
	
	if ( (done = proc__change_vmem( &ret, handle, addr, mem, size ))
		> 0 ) goto success;
	if ( (done = proc__change_file( err, handle, addr, mem, size ))
		> 0 ) goto success;
	if ( (done = proc__change_self( err, handle, addr, mem, size ))
		> 0 ) goto success;
	if ( (done = proc__change_seek( err, handle, addr, mem, size ))
		> 0 ) goto success;
	
	ptrace( PTRACE_DETACH, handle->notice.entryId, NULL, NULL );
	if ( proc_mapped_addr( &ret, handle, addr, perm, NULL ) == 0
		&& ret != EXIT_SUCCESS && err )
		*err = ret;
	return 0;
	success:
	ptrace( PTRACE_DETACH, handle->notice.entryId, NULL, NULL );
	if ( proc_mapped_addr( &ret, handle, addr, perm, NULL ) == 0
		&& ret != EXIT_SUCCESS && err )
		*err = ret;
	return done;
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

#define PROC_GLANCE_CLASS "class_proc_glance"

int lua_proc_notice_info( lua_State *L, proc_notice_t *notice )
{
	if ( !notice )
		return 0;
	lua_newtable( L );
	push_branch_bool( L, "self", notice->self );
	push_branch_int( L, "entryId", notice->entryId );
	push_branch_int( L, "ownerId", notice->ownerId );
	push_branch_str( L, "name", (char*)(notice->name.block) );
	push_branch_str( L, "cmdl", (char*)(notice->cmdl.block) );
	return 1;
}

int lua_proc_glance_init( lua_State *L ) {
	int underId = 0;
	proc_glance_t *glance = luaL_checkudata(L,1,PROC_GLANCE_CLASS);
	proc_notice_t *notice;
	if ( lua_isinteger(L,2) )
		underId = lua_tointeger(L,2);
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
	int ret = EXIT_SUCCESS, underId = 0;
	char const *name;
	nodes_t nodes = {0};
	proc_notice_t *notice;
	node_t i;
	
	if ( lua_isinteger(L,2) || lua_isnumber(L,2) )
		underId = lua_tonumber(L,2);
		
	if ( !lua_isstring(L,1) ) {
		lua_error_cb( L, "Invalid name" );
		return 0;
	}
	
	name = lua_tostring(L,1);
	notice = proc_locate_name( &ret, name, &nodes, underId );
	lua_newtable( L );
	for ( i = 0; i < nodes.count; ++i ) {
		lua_pushinteger(L,i+1);
		lua_newtable( L );
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
	lua_pushstring( L, "GASP_PROC_GLANCE_CLASS" );
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

void lua_proc_create_class( lua_State *L ) {
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
