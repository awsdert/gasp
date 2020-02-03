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
		ERRMSG( errno, "Check path" );
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
	char path[256] = {0};
	proc_handle_t *handle;
	long attach_ret = -1;
	
#ifdef PTRACE_SEIZE
	int ptrace_mode = PTRACE_SEIZE;
#else
	int ptrace_mode = PTRACE_ATTACH;
#endif
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
	
	/* Ensure none of these are treated as valid by proc_handle_shut() */
	handle->rdmemfd = (handle->wrmemfd) = (handle->pagesFd) = -1;
	
	if ( !proc_notice_info( err, pid, &(handle->notice) ) ) {
		proc_handle_shut( handle );
		return NULL;
	}
	
	if ( !(handle->samepid = (pid == getpid())) ) {
	#ifndef PTRACE_SEIZE
		ERRMSG( ENOSYS,
			"PTRACE_SEIZE not defined, defaulting to PTRACE_ATTACH");
	#endif
		attach_ret = ptrace( ptrace_mode, pid, NULL, NULL );
		if ( attach_ret != 0 ) {
			proc_handle_shut( handle );
			if ( err ) *err = errno;
			ERRMSG( errno, "Couldn't attach gasp to process" );
			return NULL;
		}
		handle->ptrace_mode = ptrace_mode;
	}
	
	(void)sprintf( path, "/proc/%d/maps", pid );
	if ( (handle->pagesFd = open(path,O_RDONLY)) < 0 ) {
		proc_handle_shut( handle );
		if ( err ) *err = errno;
		ERRMSG( errno, "Couldn't open maps file" );
		return NULL;
	}
	
	(void)sprintf( path, "/proc/%d/mem", pid );
	if ( access( path, F_OK ) == 0 && access( path, R_OK ) == 0 ) {
		if ( (handle->rdmemfd = open(path,O_RDONLY)) < 0
		)
		{
			proc_handle_shut( handle );
			if ( err ) *err = errno;
			ERRMSG( errno, "Couldn't open mem file in read mode" );
			return NULL;
		}
		
		if ( access(path,W_OK) == 0 &&
			(handle->wrmemfd = open( path, O_WRONLY )) < 0
		)
		{
			proc_handle_shut( handle );
			if ( err ) *err = errno;
			ERRMSG( errno, "Couldn't open mem file in read mode" );
			return NULL;
		}
	}
	
	handle->running = 1;
	
	if ( ptrace_mode != PTRACE_ATTACH ) {
		if ( pthread_create( &(handle->thread),
			NULL, proc_handle_wait, handle ) ) {
			proc_handle_shut( handle );
			ERRMSG( errno, "Couldn't create pthread to check for SIGKILL" );
			return NULL;
		}
		handle->waiting = 1;
	}
	
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
	
	/* None of these need to be open anymore */
	if ( handle->wrmemfd >= 0 ) close( handle->wrmemfd );
	if ( handle->rdmemfd >= 0 ) close( handle->rdmemfd );
	if ( handle->pagesFd >= 0 ) close( handle->pagesFd );
	if ( handle->ptrace_mode )
		ptrace(PTRACE_DETACH, handle->notice.entryId, NULL, NULL);
	
	/* Cleanup noticed info */
	proc_notice_zero( &(handle->notice) );
	
	/* Ensure that even if handle is reused by accident the most that
	 * can happen is a segfault */
	(void)memset(handle,0,sizeof(proc_handle_t));
	
	/* Ensure none of these are treated as valid in the above scenario*/
	handle->rdmemfd = (handle->wrmemfd) = (handle->pagesFd) = -1;
	
	/* No need for the handle anymore */
	free(handle);
}

#define PAGE_LINE_SIZE ((sizeof(void*) * 4) + 7)

intptr_t proc_mapped_next(
	int *err, proc_handle_t *handle,
	intptr_t addr, proc_mapped_t *mapped, int prot
)
{
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
	
	if ( gasp_lseek( handle->pagesFd, 0, SEEK_SET ) < 0 ) {
		if ( err ) *err = errno;
		ERRMSG( errno, "Couldn't seek page information" );
		return -1;
	}
	
	do {
		memset( mapped, 0, sizeof(proc_mapped_t) );
		do {
			memset( line, 0, PAGE_LINE_SIZE );
			if ( gasp_read( handle->pagesFd, line, PAGE_LINE_SIZE )
				!= PAGE_LINE_SIZE ) {
				if ( err ) *err = errno;
				return -1;
			}
			
			line[PAGE_LINE_SIZE-1] = 0;
			sscanf( line, "%p-%p",
				(void**)&(mapped->base),
				(void**)&(mapped->upto) );

			if ( addr < mapped->upto )
				break;
			
			/* Move file pointer to next line */
			while ( gasp_read( handle->pagesFd, line, 1 ) == 1 ) {
				if ( line[0] == '\n' )
					break;
				line[0] = 0;
			}
			
			/* Ensure we break out when we hit EOF */
			if ( line[0] != '\n' ) {
				if ( err ) *err = errno;
				return -1;
			}
		} while ( addr >= mapped->upto );
		
		pos = strchr( line, ' ' );
		*pos = 0;
		++pos;
		mapped->size = (mapped->upto) - (mapped->base);
		mapped->prot |= (pos[0] == 'r') ? 04 : 0;
		mapped->prot |= (pos[1] == 'w') ? 02 : 0;
		mapped->prot |= (pos[2] == 'x') ? 01 : 0;
		mapped->prot |= (pos[3] == 'p') ? 0 : 010;
		if ( mapped->prot | 010 )
			mapped->prot |= (pos[3] == 's') ? 0 : 020;
		sprintf( path, "/proc/%d/map_files/%s",
			handle->notice.entryId, line );
		mapped->perm = file_glance_perm( err, path );
		addr = mapped->upto;
	} while ( (mapped->prot & prot) != prot );
	return mapped->base;
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
		sprintf( path, "/proc/%d/map_files/%llx-%llx",
			handle->notice.entryId,
			(long long)(mapped.base), (long long)(mapped.upto) );
#else
		sprintf( path, "/proc/%d/map_files/%lx-%lx",
			handle->notice.entryId,
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
	intptr_t from, intptr_t upto
)
{
	int ret = EXIT_SUCCESS;
	node_t count = 0;
	uchar buff[BUFSIZ*2] = {0}, *i, *next;
	intptr_t done;
	proc_mapped_t mapped = {0};
	_Bool load_next = 0;
	
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
	
	if ( proc_mapped_next( &ret, handle, from, &mapped, 04 ) < 0 ) {
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
				mapped.upto, &mapped, 04 ) < 0 ) {
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
	int *err, char *name, nodes_t *nodes, int underId )
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
	
	for ( notice = proc_glance_open( &ret, &glance, underId )
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
	
	proc_glance_shut( &glance );
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

void proc_glance_shut( proc_glance_t *glance ) {
	if ( !glance ) return;
	(void)free_nodes( int, NULL, &(glance->idNodes) );
	proc_notice_zero( &(glance->notice) );
	(void)memset( glance, 0, sizeof(proc_glance_t) );
}

void proc_notice_puts( proc_notice_t *notice ) {
	if ( !notice )
		return;
	fprintf( stderr, "%s() Process %04X under %04X is named '%s'\n",
		__func__, notice->entryId, notice->ownerId, notice->name.block );
}

proc_notice_t* proc_notice_info(
	int *err, int pid, proc_notice_t *notice )
{
	int fd;
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
	
	sprintf( path, "/proc/%d/cmdline", pid );
	
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
	
	sprintf( path, "/proc/%d/status", pid );
	
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
	
	if ( gasp_read(fd, full->block, full->given) < size ) {
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
		proc_glance_shut( glance );
		return NULL;
	}
	
	if ( !(glance->idNodes.count) ) {
		if ( err ) *err = ENODATA;
		ERRMSG( ENODATA, "glance->idNodes.count was 0" );
		proc_glance_shut( glance );
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
	proc_glance_shut( glance );
	return NULL;
}

proc_notice_t*
	proc_glance_open( int *err, proc_glance_t *glance, int underId ) {
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
	intptr_t addr, void *mem, size_t size ) {
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
	intptr_t done = 0;
	int ret;
	
	if ( (ret = proc__rwvmem_test(handle, mem,size)) != EXIT_SUCCESS )
	{
		if ( err ) *err = ret;
		return 0;
	}
	if ( (done = gasp_pread( handle->rdmemfd, mem, size, addr )) > 0 )
		return done;
	if ( err ) *err = errno;
	return 0;
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
	
	if ( handle->samepid ) {
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
	intptr_t done = 0;
	gasp_off_t off;
	int ret;
	
	if ( (ret = proc__rwvmem_test(handle, mem,size)) != EXIT_SUCCESS )
	{
		if ( err ) *err = ret;
		return 0;
	}
	
	off = gasp_lseek( handle->rdmemfd, 0, SEEK_CUR );
	if ( errno != EXIT_SUCCESS ) {
		if ( err ) *err = errno;
		ERRMSG( errno, "Couldn't seek VM address" );
		return 0;
	}
	
	gasp_lseek( handle->rdmemfd, addr, SEEK_SET );
	if ( errno != EXIT_SUCCESS ) {
		if ( err ) *err = errno;
		ERRMSG( errno, "Couldn't seek VM address" );
		return 0;
	}
	
	done = gasp_read( handle->rdmemfd, mem, size );
	if ( done <= 0 || errno != EXIT_SUCCESS ) {
		if ( err ) *err = errno;
		ERRMSG( errno, "Couldn't read VM" );
		return 0;
	}

	gasp_lseek( handle->rdmemfd, off, SEEK_SET );
	return done;
}

intptr_t proc_glance_data(
	int *err, proc_handle_t *handle,
	intptr_t addr, void *mem, size_t size
)
{
	intptr_t done;
	mode_t perm;
	int ret = EXIT_SUCCESS, fd = -1;

	errno = EXIT_SUCCESS;
	
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
	
	if ( (done = proc__glance_file( err, handle, addr, mem, size ))
		> 0 ) goto success;
	if ( (done = proc__glance_self( err, handle, addr, mem, size ))
		> 0 ) goto success;
	if ( (done = proc__glance_seek( err, handle, addr, mem, size ))
		> 0 ) goto success;
	if ( (done = proc__glance_vmem( err, handle, addr, mem, size ))
		> 0 ) goto success;
	if ( proc_mapped_addr( &ret, handle, addr, perm, NULL ) == 0
		&& ret != EXIT_SUCCESS && err )
		*err = ret;
	return 0;
	success:
	if ( proc_mapped_addr( &ret, handle, addr, perm, NULL ) == 0
		&& ret != EXIT_SUCCESS && err )
		*err = ret;
	return done;
}

size_t proc__change_vmem(
	int *err, proc_handle_t *handle,
	intptr_t addr, void *mem, size_t size ) {
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
	intptr_t done = 0;
	int ret;
	
	if ( (ret = proc__rwvmem_test(handle, mem,size)) != EXIT_SUCCESS )
	{
		if ( err ) *err = ret;
		return 0;
	}
	if ( (done = gasp_pwrite( handle->wrmemfd, mem, size, addr )) > 0 )
		return done;
	if ( err ) *err = errno;
	return 0;
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
	
	if ( handle->samepid ) {
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
	intptr_t done = 0;
	gasp_off_t off;
	int ret;
	
	if ( (ret = proc__rwvmem_test(handle, mem,size)) != EXIT_SUCCESS )
	{
		if ( err ) *err = ret;
		return 0;
	}
	
	off = gasp_lseek( handle->wrmemfd, 0, SEEK_CUR );
	if ( errno != EXIT_SUCCESS ) {
		if ( err ) *err = errno;
		ERRMSG( errno, "Couldn't seek VM address" );
		return 0;
	}
	
	gasp_lseek( handle->wrmemfd, addr, SEEK_SET );
	if ( errno != EXIT_SUCCESS ) {
		if ( err ) *err = errno;
		ERRMSG( errno, "Couldn't seek VM address" );
		return 0;
	}
	
	done = gasp_write( handle->wrmemfd, mem, size );
	if ( done <= 0 || errno != EXIT_SUCCESS ) {
		if ( err ) *err = errno;
		ERRMSG( errno, "Couldn't read VM" );
		return 0;
	}

	gasp_lseek( handle->wrmemfd, off, SEEK_SET );
	return done;
}

intptr_t proc_change_data(
	int *err, proc_handle_t *handle,
	intptr_t addr, void *mem, size_t size
)
{
	intptr_t done;
	mode_t perm;
	int ret = EXIT_SUCCESS, fd = -1;

	errno = EXIT_SUCCESS;
	
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
	if ( ret == EADDRNOTAVAIL ) ret = EXIT_SUCCESS;
	if ( (done = proc__change_file( err, handle, addr, mem, size ))
		> 0 ) goto success;
	if ( (done = proc__change_self( err, handle, addr, mem, size ))
		> 0 ) goto success;
	if ( (done = proc__change_seek( err, handle, addr, mem, size ))
		> 0 ) goto success;
	
	if ( proc_mapped_addr( &ret, handle, addr, perm, NULL ) == 0
		&& ret != EXIT_SUCCESS && err )
		*err = ret;
	return 0;
	success:
	if ( proc_mapped_addr( &ret, handle, addr, perm, NULL ) == 0
		&& ret != EXIT_SUCCESS && err )
		*err = ret;
	return done;
}
