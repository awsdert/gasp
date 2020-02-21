#include "gasp.h"
int proc_handle_stop( proc_handle_t *handle ) {
	int ret = 0;
	(void)kill( handle->notice.entryId, SIGSTOP );
	(void)waitpid( handle->notice.entryId, &ret, WUNTRACED );
	handle->running = 0;
	return 0;
}
int proc_handle_cont( proc_handle_t *handle ) {
	if ( kill( handle->notice.entryId, SIGCONT ) != 0 )
		return errno;
	handle->running = 1;
	return 0;
}
int proc_handle_grab( proc_handle_t *handle ) {
	int ret = 0;
	if ( !(handle->attached) ) {
		errno = EXIT_SUCCESS;
		if ( ptrace( PTRACE_ATTACH,
			handle->notice.entryId, NULL, NULL ) != 0 )
			return errno;
		(void)waitpid( handle->notice.entryId, &ret, WSTOPPED );
		ret = 0;
		handle->attached = 1;
	}
	return ret;
}
int proc_handle_free( proc_handle_t *handle ) {
	if ( handle->attached ) {
		if ( ptrace( PTRACE_DETACH,
			handle->notice.entryId, NULL, NULL ) != 0 )
			return errno;
		handle->attached = 0;
	}
	return 0;
}
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
	
	errno = EXIT_SUCCESS;
	if ( (fd = open(path,O_RDONLY)) < 0 ) {
		if ( err ) *err = errno;
		switch ( errno ) {
		case ENOENT: case ENOTDIR: break;
		default:
			ERRMSG( errno, "Don't have read access path" );
		}
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

void* proc_handle_exit( void *data ) {
	proc_handle_t *handle = data;
	handle->waiting = 1;
	while ( handle->waiting ) {
		if ( access( handle->procdir, F_OK ) != 0 ) {
			handle->exited = 1;
			handle->waiting = 0;
		}
	}
	return data;
}
proc_handle_t* proc_handle_open( int *err, int pid )
{
	proc_handle_t *handle;
#ifdef gasp_pread
	char path[64] = {0};
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
	handle->perm = 0;
	handle->memfd = -1;
	
	if ( !proc_notice_info( err, pid, &(handle->notice) ) ) {
		proc_handle_shut( handle );
		return NULL;
	}
	
#if 1
	if ( pthread_create( &(handle->thread), NULL,
		proc_handle_exit, handle ) != 0
	)
	{
		if ( err ) *err = errno;
		ERRMSG( errno, "Couldn't start thread for handle" );
		proc_handle_shut( handle );
		return NULL;
	}
#endif
	
	if ( handle->notice.self )
		sprintf( handle->procdir, "/proc/self" );
	else
		sprintf( handle->procdir, "/proc/%d", pid );
	/* We don't need it stopped yet, prevent it from
	* being treated as a hanging app and killed for it,
	* also helps cleanup after crashes or being
	* forcably killed */
	proc_handle_cont( handle );
#ifdef gasp_pread
	sprintf( path, "%s/mem", handle->procdir );
	handle->memfd = open( path, O_RDWR );
	if ( handle->memfd >= 0 )
		handle->perm = O_RDWR;
	else {
		handle->memfd = open( path, O_RDONLY );
		if ( handle->memfd >= 0 )
			handle->perm = 0;
	}
#endif
	
	return handle;
}

void proc_handle_shut( proc_handle_t *handle ) {
	
	/* Don't try to close NULL as that will segfault */
	if ( !handle ) return;
	
	/* Ensure thread closes before we release the handle it's using */
	if ( handle->waiting )
		handle->waiting = 0;
	
	if ( handle->memfd >= 0 )
		close( handle->memfd );
	
	proc_handle_free( handle );
	proc_handle_cont( handle );
	
	/* Cleanup noticed info */
	proc_notice_zero( &(handle->notice) );
	
	/* Ensure that even if handle is reused by accident the most that
	 * can happen is a segfault */
	(void)memset(handle,0,sizeof(proc_handle_t));
	handle->memfd = -1;
	
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
	intptr_t addr, mode_t perm, int *fd, proc_mapped_t *Mapped
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
			if ( Mapped ) *Mapped = mapped;
			return mapped.perm;
		}
	}
	if ( err ) *err = ERANGE;
	if ( Mapped ) memset( Mapped, 0, sizeof(proc_mapped_t) );
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
	}
	
	return ret;
}

typedef struct tscan {
	bool wants2read;
	bool ready2wait;
	bool wants2free;
	bool writeable;
	pthread_t thread;
	int ret;
	int prev_fd;
	int next_fd;
	proc_handle_t *handle;
	scan_t scan;
	void * array;
	intptr_t bytes;
	intptr_t from;
	intptr_t upto;
} tscan_t;

void* aobscanthread( void *_tscan ) {
	tscan_t *tscan = (tscan_t*)_tscan;
	int clear = ~0;
	uchar *buff = NULL, *array;
	intptr_t done = 0, prev = 0, addr = 0, stop = 0;
	proc_mapped_t mapped = {0};
	scan_t *scan;
	space_t *space;
	mode_t prot = 04;
	node_t a;
	
	if ( tscan ) {
		errno = EDESTADDRREQ;
		ERRMSG( errno, "taobscan was given NULL" );
		return NULL;
	}
	tscan->ret = errno = EXIT_SUCCESS;
	array = (uchar*)(tscan->array);
	scan = &(tscan->scan);
	prot |= tscan->writeable ? 02 : 0;
	
	if ( (tscan->ret =
		proc__rwvmem_test(tscan->handle, tscan->array, tscan->bytes ))
		!= EXIT_SUCCESS )
		return _tscan;
	
	if ( !scan ) {
		tscan->ret = EDESTADDRREQ;
		return _tscan;
	}
	
	scan->total = scan->count = 0;
	space = &(scan->space);
	
	if ( tscan->next_fd < 0 ) {
		tscan->ret = EBADF;
		return _tscan;
	}
	
	if ( gasp_lseek( tscan->next_fd, 0, SEEK_SET ) < 0 ) {
		tscan->ret = errno;
		return _tscan;
	}
	
	/* Decide how we will clear memory */
	for ( addr = 0; addr < tscan->bytes; ++addr ) {
		if ( array[addr] ) {
			clear = 0;
			break;
		}
	}
	
	/* Reduce corrupted results */
	(void)memset( space->block, clear, space->given );
	
	if ( tscan->prev_fd < 0 ) {
		while (
			proc_mapped_next(
				&(tscan->ret), tscan->handle,
				mapped.upto, &mapped, prot ) >= 0
		)
		{
			if ( tscan->wants2free ) {
				scan->total = scan->count;
				return _tscan;
			}
			if ( tscan->wants2read ) {
				tscan->ready2wait = 1;
				while ( tscan->wants2read );
				tscan->ready2wait = 0;
			}
			/* Skip irrelevant regions */
			if ( mapped.base < tscan->from
				|| mapped.upto <= tscan->from
				|| mapped.base >= tscan->upto
				|| mapped.upto > tscan->upto
				|| (mapped.prot & prot) != prot )
				continue;

			/* If continued page lines up with previous then move the bytes
			 * we skipped to front of buffer to be compared now that we
			 * will have the bytes after them */
			addr = 0;
			if ( prev > 0 && prev == mapped.base )
				memmove( buff, buff + stop, (addr = tscan->bytes) );

			/* Ensure we have enough memory to read into */
			stop = mapped.upto - mapped.base;
			if ( !(buff = more_space(
				&(tscan->ret), space, addr + stop ))
			)
			{
				scan->total = scan->count;
				return _tscan;
			}
			
			/* Reduce corrupted results */
			(void)memset( buff + addr, clear, stop );
			
			/* Safe to read the memory block now */
			if ( !proc_glance_data(
				&(tscan->ret), tscan->handle,
				mapped.base, buff + addr, stop )
			)
			{
				scan->total = scan->count;
				return _tscan;
			}
			stop += addr;
			stop -= tscan->bytes;
			for ( addr = 0; addr < stop; ++addr ) {
				if ( memcmp( buff + addr, array, tscan->bytes ) == 0 ) {
					prev = mapped.base + addr;
					if ( write( tscan->next_fd, &prev, sizeof(void*) )
						< (ssize_t)sizeof(void*)
					)
					{
						tscan->ret = errno;
						scan->total = scan->count;
						return _tscan;
					}
					scan->count++;
				}
			}
			prev = mapped.upto;
		}
	}
	else {
		if ( gasp_lseek( tscan->prev_fd, 0, SEEK_SET ) < 0 ) {
			tscan->ret = errno;
			return _tscan;
		}
		for ( a = 0; a < scan->total; ++a ) {
			if ( read( tscan->prev_fd, &done,
				sizeof(void*) ) != sizeof(void*)
			)
			{
				tscan->ret = errno;
				scan->total = scan->count;
				return _tscan;
			}
			if ( done >= mapped.base && done <= mapped.upto )
				goto check_next_address;
			while ( proc_mapped_next(
				&(tscan->ret), tscan->handle,
				mapped.upto, &mapped, prot ) >= 0
			)
			{
				if ( tscan->wants2free ) {
					scan->total = scan->count;
					return _tscan;
				}
				if ( tscan->wants2read ) {
					tscan->ready2wait = 1;
					while ( tscan->wants2read );
					tscan->ready2wait = 0;
				}
				/* Skip irrelevant regions */
				if ( mapped.base < tscan->from
					|| mapped.upto <= tscan->from
					|| mapped.base >= tscan->upto
					|| mapped.upto > tscan->upto
					|| (mapped.prot & prot) != prot )
					continue;
	check_next_address:
				/* If continued page lines up with previous then move the bytes
				 * we skipped to front of buffer to be compared now that we
				 * will have the bytes after them */
				addr = 0;
				if ( prev > 0 && prev == mapped.base )
					memmove( buff, buff + stop, (addr = tscan->bytes) );

				/* Ensure we have enough memory to read into */
				stop = mapped.upto - mapped.base;
				if ( !(buff = more_space(
					&(tscan->ret), space, addr + stop )) ) {
					scan->total = scan->count;
					return _tscan;
				}
				
				/* Reduce corrupted results */
				(void)memset( buff + addr, clear, stop );
				
				/* Safe to read the memory block now */
				if ( !proc_glance_data(
					&(tscan->ret), tscan->handle,
					mapped.base, buff + addr, stop )
				)
				{
					scan->total = scan->count;
					return _tscan;
				}
				stop += addr;
				stop -= tscan->bytes;
				addr = done - mapped.base;
				if ( memcmp( buff + addr, array, tscan->bytes ) == 0 ) {
					if ( write( tscan->next_fd, &done, sizeof(void*) )
						< (ssize_t)(sizeof(void*))
					)
					{
						tscan->ret = errno;
						scan->total = scan->count;
						return _tscan;
					}
					scan->count++;
				}
				prev = mapped.upto;
			}
		}
	}
	tscan->ret = EXIT_SUCCESS;
	scan->total = scan->count;
	return _tscan;
}

node_t proc_aobscan(
	int *err, int prev_fd, int next_fd, scan_t *scan,
	proc_handle_t *handle,
	uchar *array, intptr_t bytes,
	intptr_t from, intptr_t upto,
	bool writable
)
{
	int ret = EXIT_SUCCESS, clear = ~0;
	uchar *buff = NULL;
	intptr_t done = 0, prev = 0, addr = 0, stop = 0;
	proc_mapped_t mapped = {0};
	space_t *space;
	node_t a;
	node_t pages = 0, max_pages = 1000;
	mode_t prot = 04 | (writable ? 02 : 0);
	
	errno = EXIT_SUCCESS;
	
	if ( (ret = proc__rwvmem_test( handle, array, bytes ))
		!= EXIT_SUCCESS ) {
		if ( err ) *err = ret;
		return 0;
	}
	
	if ( !scan ) {
		ret = EDESTADDRREQ;
		if ( err ) *err = ret;
		ERRMSG( ret, "Need scan object to optimize memory usage" );
		return 0;
	}
	
	scan->count = 0;
	space = &(scan->space);
	
	if ( prev_fd < 0 || next_fd < 0 ) {
		ret = EDESTADDRREQ;
		if ( err ) *err = ret;
		ERRMSG( ret, "init_fd needs a file descriptor" );
		return 0;
	}
	
	if ( gasp_lseek( prev_fd, 0, SEEK_SET ) < 0
		|| gasp_lseek( next_fd, 0, SEEK_SET ) < 0 ) {
		if ( err ) *err = errno;
		ERRMSG( errno, "failed reset position for output list file" );
		return 0;
	}
	
	/* Decide how we will clear memory */
	for ( addr = 0; addr < bytes; ++addr ) {
		if ( array[addr] ) {
			clear = 0;
			break;
		}
	}
	
	/* Reduce corrupted results */
	(void)memset( space->block, clear, space->given );
	
	for ( a = 0; a < scan->total; ++a ) {
		if ( read( prev_fd, &done, sizeof(void*) ) != sizeof(void*) ) {
			ret = errno;
			if ( err ) *err = ret;
			scan->total = scan->count;
			return scan->count;
		}
		if ( done >= mapped.base && done <= mapped.upto )
			goto check_next_address;
		while ( pages < max_pages && proc_mapped_next(
			&ret, handle, mapped.upto, &mapped, prot ) >= 0
		)
		{
			++pages;
			/* Skip irrelevant regions */
			if ( mapped.base < from
				|| mapped.upto <= from
				|| mapped.base >= upto
				|| mapped.upto > upto
				|| (mapped.prot & prot) != prot )
				continue;
check_next_address:
			/* If continued page lines up with previous then move the bytes
			 * we skipped to front of buffer to be compared now that we
			 * will have the bytes after them */
			addr = 0;
			if ( prev > 0 && prev == mapped.base )
				memmove( buff, buff + stop, (addr = bytes) );

			/* Ensure we have enough memory to read into */
			stop = mapped.upto - mapped.base;
			if ( !(buff = more_space( &ret, space, addr + stop )) ) {
				if ( err ) *err = ret;
				scan->total = scan->count;
				return scan->count;
			}
			
			/* Reduce corrupted results */
			(void)memset( buff + addr, clear, stop );
			
			/* Safe to read the memory block now */
			if ( !proc_glance_data( &ret, handle,
				mapped.base, buff + addr, stop ) ) {
				if ( err ) *err = ret;
				ERRMSG( ret, "Couldn't read memory" );
				scan->total = scan->count;
				return scan->count;
			}
			stop += addr;
			stop -= bytes;
			addr = done - mapped.base;
			if ( memcmp( buff + addr, array, bytes ) == 0 ) {
				if ( write( next_fd, &done, sizeof(void*) )
					< (ssize_t)(sizeof(void*))
				)
				{
					ret = errno;
					if ( err ) *err = ret;
					scan->total = scan->count;
					return scan->count;
				}
				scan->count++;
			}
			prev = mapped.upto;
		}
	}
	if ( err ) *err = EXIT_SUCCESS;
	scan->total = scan->count;
	return scan->count;
}

node_t proc_aobinit(
	int *err, int init_fd, scan_t *scan,
	proc_handle_t *handle,
	uchar *array, intptr_t bytes,
	intptr_t from, intptr_t upto,
	bool writable
)
{
	int ret = EXIT_SUCCESS, clear = ~0;
	uchar *buff = NULL;
	intptr_t prev = 0, addr = 0, stop = 0;
	proc_mapped_t mapped = {0};
	space_t *space;
	mode_t prot = 04 | (writable ? 02 : 0);
	
	errno = EXIT_SUCCESS;
	
	if ( (ret = proc__rwvmem_test( handle, array, bytes ))
		!= EXIT_SUCCESS ) {
		if ( err ) *err = ret;
		return 0;
	}
	
	if ( !scan ) {
		ret = EDESTADDRREQ;
		if ( err ) *err = ret;
		ERRMSG( ret, "Need scan object to optimize memory usage" );
		return 0;
	}
	
	scan->total = scan->count = 0;
	space = &(scan->space);
	
	if ( init_fd < 0 ) {
		ret = EDESTADDRREQ;
		if ( err ) *err = ret;
		ERRMSG( ret, "init_fd needs a file descriptor" );
		return 0;
	}
	
	if ( gasp_lseek( init_fd, 0, SEEK_SET ) < 0 ) {
		if ( err ) *err = errno;
		ERRMSG( errno, "failed reset position for output list file" );
		return 0;
	}
	
	/* Decide how we will clear memory */
	for ( addr = 0; addr < bytes; ++addr ) {
		if ( array[addr] ) {
			clear = 0;
			break;
		}
	}
	
	/* Reduce corrupted results */
	(void)memset( space->block, clear, space->given );
	
	while (
		proc_mapped_next(
			&ret, handle, mapped.upto, &mapped, prot ) >= 0
	)
	{
		/* Skip irrelevant regions */
		if ( mapped.base < from
			|| mapped.upto <= from
			|| mapped.base >= upto
			|| mapped.upto > upto
			|| (mapped.prot & prot) != prot )
			continue;

		/* If continued page lines up with previous then move the bytes
		 * we skipped to front of buffer to be compared now that we
		 * will have the bytes after them */
		addr = 0;
		if ( prev > 0 && prev == mapped.base )
			memmove( buff, buff + stop, (addr = bytes) );

		/* Ensure we have enough memory to read into */
		stop = mapped.upto - mapped.base;
		if ( !(buff = more_space( &ret, space, addr + stop )) ) {
			if ( err ) *err = ret;
			ERRMSG( ret, "Couldn't allocate memory" );
			scan->total = scan->count;
			return scan->count;
		}
		
		/* Reduce corrupted results */
		(void)memset( buff + addr, clear, stop );
		
		/* Safe to read the memory block now */
		if ( !proc_glance_data( &ret, handle,
			mapped.base, buff + addr, stop ) ) {
			if ( err ) *err = ret;
			ERRMSG( ret, "Couldn't read memory" );
			scan->total = scan->count;
			return scan->count;
		}
		stop += addr;
		stop -= bytes;
		for ( addr = 0; addr < stop; ++addr ) {
			if ( memcmp( buff + addr, array, bytes ) == 0 ) {
				prev = mapped.base + addr;
				if ( write( init_fd, &prev, sizeof(void*) )
					< (ssize_t)sizeof(void*)
				)
				{
					ret = errno;
					if ( err ) *err = ret;
					ERRMSG( ret, "Couldn't write an address"
						" to output file" );
					scan->total = scan->count;
					return scan->count;
				}
				scan->count++;
			}
		}
		prev = mapped.upto;
	}
	if ( err ) *err = EXIT_SUCCESS;
	scan->total = scan->count;
	return scan->count;
}

char const * substr( char const * src, char const *txt ) {
	size_t i, s, t, slen =
		src ? strlen(src) : 0, tlen = txt ? strlen(txt): 0;
	if ( tlen > slen )
		return NULL;
	for ( s = 0; s < slen; ++s ) {
		for ( i = s, t = 0; i < slen && t < tlen; ++i, ++t ) {
			if ( src[i] != txt[t] )
				break;
		}
		if ( t >= tlen )
			return src + s;
	}
	return NULL;
}
char const * substri( char const * src, char const *txt ) {
	size_t i, s, t, slen =
		src ? strlen(src) : 0, tlen = txt ? strlen(txt): 0;
	if ( tlen > slen )
		return NULL;
	for ( s = 0; s < slen; ++s ) {
		for ( i = s, t = 0; i < slen && t < tlen; ++i, ++t ) {
			if ( toupper(src[i]) != toupper(txt[t]) )
				break;
		}
		if ( t >= tlen )
			return src + s;
	}
	return NULL;
}

proc_notice_t* proc_locate_name(
	int *err, char const *name, nodes_t *nodes, int underId,
		bool usecase )
{
	int ret = EXIT_SUCCESS;
	node_t i = 0;
	size_t size;
	proc_glance_t glance;
	proc_notice_t *notice, *noticed;
	char *text;
	char const * (*instr)( char const *src, char const *txt )
		= usecase ? substr : substri;
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
		if ( name && *name ) {
			text = notice->name.block;
			if ( !instr( text, name ) ) {
				text = notice->cmdl.block;
				if ( !instr( text, name ) )
					continue;
			}
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
	if ( nodes->count > 0 )
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
	
	if ( !more_space( &ret, full, size * 2 )) {
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
	
	k = full->block;
	while ( size > 0 ) {
		n = strchr( k, '\n' );
		v = strchr( k, ':' );
		if ( !v ) {
			if ( n ) {
				k = ++n;
				continue;
			}
			break;
		}
		if ( n )
			*n = 0;
		size -= strlen(k) + 1;
		*v = 0;
		/* We only care about the name and ppid at the moment */
		if ( strcmp( k, "Name" ) == 0 )
			memcpy( name->block, v + 2, strlen(v+1) );
		if ( strcmp( k, "PPid" ) == 0 )
			notice->ownerId = strtol( v + 2, NULL, 10 );
		*v = ':';
		if ( n )
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

size_t proc__glance_peek(
	int *err, proc_handle_t *handle,
	intptr_t addr, void *mem, intptr_t size ) {
	intptr_t done = 0, dst = 0, predone = 0;
	int ret;
	uchar *m = mem;
	size_t temp;
	
	if ( (ret = proc__rwvmem_test(handle, mem,size)) != EXIT_SUCCESS )
	{
		if ( err ) *err = ret;
		return 0;
	}

	errno = EXIT_SUCCESS;
	
	for ( temp = 0; addr % sizeof(long); ++temp, --addr );
	if ( temp ) {
		dst = ptrace( PTRACE_PEEKDATA,
			handle->notice.entryId, (void*)addr, NULL );
		if ( errno != EXIT_SUCCESS )
			return 0;
		temp = (sizeof(long) - (predone = temp));
		memcpy( m, (&dst) + predone, temp );
		addr += sizeof(long);
		size -= predone;
	}
	
	while ( done < size ) {
		dst = ptrace( PTRACE_PEEKTEXT,
			handle->notice.entryId, (void*)(addr + done), NULL );
		if ( errno != EXIT_SUCCESS )
			return done + predone;
		temp = size - done;
		if ( temp > sizeof(long) ) temp = sizeof(long);
		memcpy( m + predone + done, &dst, temp );
		done += temp;
	}
	
	return done + predone;
}

size_t proc__glance_vmem(
	int *err, proc_handle_t *handle,
	intptr_t addr, void *mem, intptr_t size ) {
#ifdef _GNU_SOURCE
	struct iovec internal, external;
	intptr_t done, predone = 0;
	int ret;
	uchar *m = mem;
	
	if ( (ret = proc__rwvmem_test(handle, mem,size)) != EXIT_SUCCESS )
	{
		if ( err ) *err = ret;
		return 0;
	}

	errno = EXIT_SUCCESS;
#if 0
	if ( addr % 0xFF ) {
		predone = proc__glance_peek( &ret, handle, addr, mem,
			0xFF - (addr % 0xFF) );
		if ( ret != EXIT_SUCCESS ) {
			if ( err ) *err = ret;
			ERRMSG( ret, "Couldn't read from VM" );
			fprintf( stderr, "%p\n", (void*)addr );
			return predone;
		}
		addr += predone;
		size -= predone;
	}
#endif
	internal.iov_base = m + predone;
	internal.iov_len = size;
	external.iov_base = (void*)addr;
	external.iov_len = size;
	
	if ( (done = process_vm_readv(
		handle->notice.entryId, &internal, 1, &external, 1, 0))
		<= 0 )
	{
		if ( err ) *err = errno;
		ERRMSG( errno, "Couldn't glance at VM" );
		fprintf( stderr, "%p\n", external.iov_base );
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
	int ret, fd;
	char path[64] = {0};
	
	if ( (ret = proc__rwvmem_test(handle, mem,size)) != EXIT_SUCCESS )
	{
		if ( err ) *err = ret;
		return 0;
	}
	
	if ( handle->memfd >= 0 )
		fd = handle->memfd;
	else {
		sprintf( path, "%s/mem", handle->procdir );
		if ( (fd = open(path,O_RDONLY)) < 0 ) {
			if ( err ) *err = errno;
			return 0;
		}
	}
	
	done = gasp_pread( fd, mem, size, addr );
	if ( handle->memfd < 0 )
		close(fd);

	if ( done < 0 ) {
		if ( err ) *err = errno;
		if ( errno != ESPIPE && errno != EIO )
			ERRMSG( errno, "Couldn't read process memory" );
		return 0;
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
	
	if ( handle->memfd >= 0 )
		fd = handle->memfd;
	else {
		sprintf( path, "%s/mem", handle->procdir );
		if ( (fd = open( path, O_RDONLY )) < 0 ) {
			if ( err ) *err = errno;
			ERRMSG( errno, "Could'nt open process memory" );
			fprintf( stderr, "Path: '%s'\n", path );
			return 0;
		}
	}
	
	gasp_lseek( fd, addr, SEEK_SET );
	if ( errno != EXIT_SUCCESS )
		goto failed;
	
	done = gasp_read( fd, mem, size );
	if ( done <= 0 || errno != EXIT_SUCCESS )
		goto failed;

	if ( handle->memfd < 0 )
		close(fd);
	return done;
	
	failed:
	if ( handle->memfd < 0 )
		close(fd);
	if ( err ) *err = errno;
	if ( errno != ESPIPE && errno != EIO )
		ERRMSG( errno, "Couldn't read process memory" );
	return 0;
}

intptr_t proc_glance_data(
	int *err, proc_handle_t *handle,
	intptr_t addr, void *mem, size_t size
)
{
	intptr_t done;
	int ret = proc__rwvmem_test( handle, mem, size );
	
	if ( ret != EXIT_SUCCESS ) {
		if ( err ) *err = ret;
		return 0;
	}
	
	errno = EXIT_SUCCESS;
	if ( (ret = proc_handle_stop(handle)) != 0 ) {
		if ( err ) *err = ret;
		ERRMSG( ret, "Couldn't stop process" );
		return 0;
	}
	if ( (ret = proc_handle_grab(handle)) != 0 ) {
		if ( err ) *err = ret;
		ERRMSG( ret, "Couldn't attach to process" );
		return 0;
	}
	
	/* Aviod fiddling with own memory file */
	if ( (done = proc__glance_self( err, handle, addr, mem, size ))
		> 0 ) goto success;
#if 1
	//fputs( "Next: pread()\n", stderr );
	/* gasp_pread() */
	if ( (done = proc__glance_file( err, handle, addr, mem, size ))
		> 0 ) goto success;
#endif
#if 1
	//fputs( "Next: read()\n", stderr );
	/* gasp_lseek() & gasp_read() */
	if ( (done = proc__glance_seek( err, handle, addr, mem, size ))
		> 0 ) goto success;
#endif
#if 0
	//fputs( "Next: process_vm_readv()\n", stderr );
	/* Last ditch effort to avoid ptrace */
	if ( (done = proc__glance_vmem( err, handle, addr, mem, size ))
		> 0 ) goto success;
#endif
	/* PTRACE_PEEKDATA */
	if ( (done = proc__glance_peek( err, handle, addr, mem, size ))
		> 0 ) goto success;
	
	/* Detatch and ensure continued activity */
	success:
	ret = proc_handle_free(handle);
	ret = proc_handle_cont(handle);
	return done;
}

size_t proc__change_poke(
	int *err, proc_handle_t *handle,
	intptr_t addr, void *mem, intptr_t size ) {
	intptr_t done = 0, dst = 0;
	int ret;
	uchar *m = mem;
	size_t temp;
	
	if ( (ret = proc__rwvmem_test(handle, mem,size)) != EXIT_SUCCESS )
	{
		if ( err ) *err = ret;
		return 0;
	}

	errno = EXIT_SUCCESS;
	
	while ( done < size ) {
		dst = ptrace( PTRACE_PEEKDATA,
			handle->notice.entryId, addr + done, NULL );
		temp = size - done;
		if ( temp > sizeof(long) ) temp = sizeof(long);
		memcpy( &dst, m + done, temp );
		ptrace( PTRACE_POKEDATA,
			handle->notice.entryId, addr + done, &dst );
		done += temp;
	}
	
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
	intptr_t done = 0;
	int ret, fd;
	
	(void)fd;
	if ( (ret = proc__rwvmem_test(handle, mem,size)) != EXIT_SUCCESS )
	{
		if ( err ) *err = ret;
		return 0;
	}
	
	if ( handle->perm != O_RDWR || handle->memfd < 0 )
		return 0;
	
	done = gasp_pwrite( handle->memfd, mem, size, addr );
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
		errno = EXIT_SUCCESS;
		(void)memmove( (void*)addr, mem, size );
		if ( errno != EXIT_SUCCESS ) {
			if ( err ) *err = errno;
			ERRMSG( errno, "Couldn't override VM" );
			return 0;
		}
		return size;
	}
	
	return 0;
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
	int ret = proc__rwvmem_test(handle,mem,size);

	if ( ret != EXIT_SUCCESS ) {
		if ( err ) *err = ret;
		return 0;
	}
	
	errno = EXIT_SUCCESS;
	if ( (ret = proc_handle_stop(handle)) != 0 ) {
		if ( err ) *err = ret;
		ERRMSG( ret, "Couldn't stop process" );
		return 0;
	}
	if ( (ret = proc_handle_grab(handle)) != 0 ) {
		if ( err ) *err = ret;
		ERRMSG( ret, "Couldn't attach to process" );
		return 0;
	}
	
	/* Avoid fiddling with own memory file */
	if ( (done = proc__change_self( err, handle, addr, mem, size ))
		> 0 ) goto success;
	/* gasp_pwrite() */
	if ( (done = proc__change_file( err, handle, addr, mem, size ))
		> 0 ) goto success;
	/* gasp_seek() & gasp_write() */
	if ( (done = proc__change_seek( err, handle, addr, mem, size ))
		> 0 ) goto success;
	/* Last ditch effort to avoid ptrace */
	if ( (done = proc__change_vmem( &ret, handle, addr, mem, size ))
		> 0 ) goto success;
	/* PTRACE_POKEDATA */
	if ( (done = proc__change_poke( err, handle, addr, mem, size ))
		> 0 ) goto success;
	
	success:
	ret = proc_handle_free(handle);
	ret = proc_handle_cont(handle);
	return done;
}
