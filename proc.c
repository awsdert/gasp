#include "gasp.h"

size_t file_get_size( int *err, char const *path )
{
	int fd;
	char buff[BUFSIZ];
	gasp_off_t size, tmp;
	
	errno = EXIT_SUCCESS;
	if ( !path || !(*path) ) {
		if ( err ) *err = EINVAL;
		ERRMSG( EINVAL, "path was invalid" );
		return 0;
	}
	
	if ( (fd = open(path,O_RDONLY)) < 0 ) {
		if ( err ) *err = errno;
		ERRMSG( errno, "Couldn't open file" );
		fprintf( stderr, "File '%s'\n", path );
		return 0;
	}
	
	size = gasp_lseek( fd, 0, SEEK_END );
	if ( size <= 0 ) size = gasp_lseek( fd, 0, SEEK_CUR );
	if ( size <= 0 ) {
		errno = EXIT_SUCCESS;
		while ( (tmp = read( fd, buff, BUFSIZ )) )
			size += tmp;
	}
	close(fd);
	
	if ( size <= 0 ) {
		if ( err ) *err = errno;
		ERRMSG( errno, "Couldn't get file size" );
		fprintf( stderr, "File '%s'\n", path );
		return 0;
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
proc_handle_t* proc_handle_open( int *err, int pid )
{
	char path[256] = {0};
	proc_handle_t *handle;
	long attach_ret = -1;
#ifdef PTRACE_SEIZE
	int ptrace_mode = PTRACE_SEIZE;
#else
	int ptrace_mode = PTRACE_ATTACH;
	ERRMSG( ENOSYS,
		"PTRACE_SEIZE not defined, defaulting to PTRACE_ATTACH");
#endif
	attach_ret = ptrace( ptrace_mode, pid, NULL, NULL );
	if ( attach_ret != 0 ) {
		if ( err ) *err = errno;
		return NULL;
	}
	
	if ( !(handle = calloc(sizeof(proc_handle_t),1)) ) {
		if ( err ) *err = errno;
		return NULL;
	}
	
	if ( !proc_notice_info( err, pid, &(handle->notice) ) ) {
		proc_handle_shut( handle );
		return NULL;
	}
	
	(void)sprintf( path, "/proc/%d/maps", pid );
	if ( (handle->pagesFd = open(path,O_RDONLY)) < 0 ) {
		proc_handle_shut( handle );
		if ( err ) *err = errno;
		ERRMSG( errno, "Couldn't open maps file" );
		return NULL;
	}
	
	(void)sprintf( path, "/proc/%d/mem", pid );
	if ( (handle->rdMemFd = open(path,O_RDONLY)) < 0 ) {
		proc_handle_shut( handle );
		if ( err ) *err = errno;
		ERRMSG( errno, "Couldn't open mem file in read only mode" );
		return NULL;
	}
	handle->wrMemFd = open(path,O_WRONLY);
	handle->running = 1;
#if 0
	if ( pid != (getpid()) ) {
		if ( pthread_create( &(handle->thread),
			NULL, proc_handle_wait, handle ) ) {
			proc_handle_shut( handle );
			ERRMSG( errno, "Couldn't create pthread to check for SIGKILL" );
			return NULL;
		}
		handle->waiting = 1;
	}
#endif
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
	/* None of these need to be open anymore */
	if ( handle->rdMemFd >= 0 ) close( handle->rdMemFd );
	if ( handle->wrMemFd >= 0 ) close( handle->wrMemFd );
	if ( handle->pagesFd >= 0 ) close( handle->pagesFd );
	/* Ensure that even if handle is reused by accident the most that
	 * can happen is a segfault */
	(void)memset(handle,0,sizeof(proc_handle_t));
	/* Ensure none of these are treated as valid in the above scenario*/
	handle->rdMemFd = handle->wrMemFd = handle->pagesFd = -1;
	/* No need for the handle anymore */
	free(handle);
}

#define PAGE_LINE_SIZE ((sizeof(void*) * 4) + 7)

intptr_t proc_mapped_next(
	int *err, proc_handle_t *handle,
	intptr_t addr, proc_mapped_t *mapped
)
{
	char line[PAGE_LINE_SIZE] = {0}, *perm;
	
	if ( !mapped ) {
		if ( err ) *err = EDESTADDRREQ;
		ERRMSG( EINVAL, "Need destination for page details" );
		return -1;
	}
	
	memset( mapped, 0, sizeof(proc_mapped_t) );
	mapped->size = -1;
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
		memset( line, 0, PAGE_LINE_SIZE );
		if ( gasp_read( handle->pagesFd, line, PAGE_LINE_SIZE )
			!= PAGE_LINE_SIZE ) {
			if ( errno == EXIT_SUCCESS ) errno = ERANGE;
			if ( err ) *err = errno;
			ERRMSG( errno, "Couldn't read page information" );
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
	
	perm = strstr( line, " " );
	++perm;
	mapped->size = (mapped->upto) - (mapped->base);
	mapped->perm |= (perm[0] == 'r') ? 04 : 0;
	mapped->perm |= (perm[1] == 'w') ? 02 : 0;
	mapped->perm |= (perm[2] == 'x') ? 01 : 0;
	return mapped->base;
}

int proc_mapped_addr(
	int *err, proc_handle_t *handle,
	intptr_t addr, int octal_perm
)
{
	proc_mapped_t mapped = {0};
	
	while ( proc_mapped_next( err, handle, mapped.upto, &mapped ) >= 0 ) {
		if ( mapped.upto < addr ) continue;
		return mapped.perm;
	}
	
	if ( err ) *err = ERANGE;
	ERRMSG( ERANGE, "Address out of range" );
	return 0;
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
	if ( into < 0 ) {
		if ( err ) *err = EDESTADDRREQ;
		ERRMSG( EDESTADDRREQ, "into needs a file descriptor" );
		return 0;
	}
	
	if ( !handle || !array || bytes <= 0 ) {
		if ( err ) *err = EINVAL;
		if ( !handle ) ERRMSG( EINVAL, "invalid handle" );
		if ( !array ) ERRMSG( EINVAL, "invalid array of bytes" );
		if ( bytes <= 0 ) ERRMSG( EINVAL, "invalid size for array of bytes" );
		return 0;
	}
	
	while ( mapped.upto <= from ) {
		if ( proc_mapped_next( &ret, handle,
			mapped.upto, &mapped ) < 0 ) {
			if ( err ) *err = ret;
			ERRMSG( ret, "no pages with base address >= start address" );
			return 0;
		}
	}
	
	if ( mapped.base >= (upto - bytes) ) {
		if ( err ) *err = EXIT_SUCCESS;
		return 0;
	}
	if ( from < mapped.base ) from = mapped.base;
	
	if ( gasp_lseek( into, 0, SEEK_SET ) < 0 ) {
		if ( err ) *err = errno;
		ERRMSG( errno, "failed reset position for output list file" );
		return 0;
	}
	
	done = mapped.upto - from;
	if ( done >= BUFSIZ ) done = BUFSIZ;
	if ( (done = proc_glance_data( err, handle, from, buff, done ))
		<= 0 ) {
		ERRMSG( errno, "failed to read from process memory" );
		return 0;
	}
	
	while ( (from + bytes) < upto ) {
		done = mapped.upto - from;	
		if ( done >= BUFSIZ ) {
			if ( done > BUFSIZ ) done = BUFSIZ;		
			if ( (done =
				proc_glance_data( err, handle, from,
				buff + BUFSIZ, done )) <= 0) {
				ERRMSG( errno, "failed read from process memory" );
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
			if ( proc_mapped_next( err, handle,
				mapped.upto, &mapped ) < 0 )
				return count;
			
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
		if ( !strstr( text, name ) )
			continue;
			
		if ( (ret = add_node( nodes, &i, sizeof(proc_notice_t) ))
			!= EXIT_SUCCESS )
			break;
		
		noticed = (proc_notice_t*)(nodes->space.block);
		noticed[i].entryId = notice->entryId;
		if ( !more_space(
			&ret,&(noticed[i].name), (size = strlen(text)+1)) ) {
			nodes->count--;
			break;
		}
		
		(void)memcpy( noticed[i].name.block, text, size );
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
}

void proc_glance_shut( proc_glance_t *glance ) {
	if ( !glance ) return;
	(void)free_nodes( int, NULL, &(glance->idNodes) );
	proc_notice_zero( &(glance->notice) );
	(void)memset( glance, 0, sizeof(proc_glance_t) );
}

proc_notice_t* proc_notice_info(
	int *err, int pid, proc_notice_t *notice )
{
	int fd;
	char path[256] = {0};
	space_t *name, *full;
	kvpair_t *kvpair;
	int ret;
	char *k, *v, *n;
	intptr_t size;
	
	if ( !notice ) {
		if ( err ) *err = EDESTADDRREQ;
		ERRMSG( EDESTADDRREQ, "notice was NULL" );
		return NULL;
	}
	kvpair = &(notice->kvpair);
	name = &(notice->name);
	full = &(kvpair->full);
	
	memset( name->block, 0, name->given );
	memset( full->block, 0, full->given );
	
	notice->ownerId = -1;
	notice->entryId = pid;
	
	if ( pid < 1 ) {
		ret = EDESTADDRREQ;
		if ( err ) *err = ret;
		ERRMSG( ret, "pid must not be less than 1");
		return NULL;
	}
	sprintf( path, "/proc/%d/status", pid );
	
	/* Implements a fallback looped read call in the event lseek gleans
	 * nothing */
	if ( !(size = file_get_size( &ret, path )) ) {
		if ( err ) *err = ret;
		ERRMSG( ret, "Couldn't get status size" );
		fprintf( stderr, "File '%s'\n", path );
		return NULL;
	}
	
	if ( (fd = open(path,O_RDONLY)) < 0 ) {
		if ( err ) *err = errno;
		ERRMSG( errno, "Couldn't open status file" );
		fprintf( stderr, "File '%s'\n", path );
		return NULL;
	}
	
	if ( !more_space( &ret, full, size )) {
		if ( err ) *err = ret;
		ERRMSG( ret, "Couldn't allocate memory to read into" );
		fprintf( stderr, "Wanted %zu bytes\n", size );
		close(fd);
		return NULL;
	}
	
	if ( gasp_read(fd, full->block, full->given) < size ) {
		close(fd);
		ret = ENODATA;
		if ( err ) *err = ret;
		ERRMSG( ret, "Couldn't read the status file" );
		return NULL;
	}
	/* No longer need the file opened since we have everything */
	close(fd);
	
	/* Ensure we have enough space for the name */
	if ( !more_space( &ret, name, full->given ) ) {
		if ( err ) *err = ret;
		ERRMSG( ret, "Couldn't allocate memory for name");
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
	fprintf( stderr, "pid: %d; ppid: %d; name '%s'\n",
		notice->entryId, notice->ownerId, notice->name.block );
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
	ERRMSG( ret, "glance->dir was NULL" );
	return NULL;
}

size_t proc_glance_data(
	int *err, proc_handle_t *handle,
	intptr_t addr, void *dst, size_t size ) {
	size_t done;
#ifdef gasp_pread
	errno = EXIT_SUCCESS;
	
	if ( !handle ) {
		if ( err ) *err = EINVAL;
		ERRMSG( errno, "Invalid handle" );
		return 0;
	}
	
	done = gasp_pread( handle->rdMemFd, dst, size, addr );
	if ( err ) *err = errno;
	if ( errno != EXIT_SUCCESS )
		ERRMSG( errno, "Couldn't read VM" );
	return done;
#else
	gasp_off_t off;
	errno = EXIT_SUCCESS;
	
	if ( !handle ) {
		if ( err ) *err = EINVAL;
		ERRMSG( EINVAL, "Invalid handle" );
		return 0;
	}
	
	off = gasp_lseek( handle->rdMemFd, 0, SEEK_CUR );
	if ( errno != EXIT_SUCCESS ) {
		if ( err ) *err = errno;
		ERRMSG( errno, "Couldn't seek VM address" );
		return 0;
	}
	
	gasp_lseek( handle->rdMemFd, addr, SEEK_SET );
	if ( errno != EXIT_SUCCESS ) {
		if ( err ) *err = errno;
		ERRMSG( errno, "Couldn't seek VM address" );
		return 0;
	}
	
	done = gasp_read( handle->rdMemFd, data, size );
	if ( errno != EXIT_SUCCESS ) {
		if ( err ) *err = errno;
		ERRMSG( errno, "Couldn't read VM" );
		return 0;
	}

	gasp_lseek( handle->rdMemFd, off, SEEK_SET );
#endif
	return done;
}
size_t proc_change_data(
	int *err, proc_handle_t *handle,
	intptr_t addr, void *src, size_t size ) {
	size_t done;
#ifdef gasp_pwrite
	errno = EXIT_SUCCESS;
	
	if ( !handle ) {
		if ( err ) *err = EINVAL;
		ERRMSG( errno, "Invalid handle" );
		return 0;
	}
	
	done = pread( handle->wrMemFd, src, size, addr );
	if ( err ) *err = errno;
	if ( errno != EXIT_SUCCESS )
		ERRMSG( errno, "Couldn't override VM" );
	return done;
#else
	gasp_off_t off;
	errno = EXIT_SUCCESS;
	
	if ( !handle ) {
		if ( err ) *err = EINVAL;
		ERRMSG( errno, "Invalid" );
		return 0;
	}
	
	off = gasp_lseek( handle->wrMemFd, 0, SEEK_CUR );
	if ( errno != EXIT_SUCCESS ) {
		if ( err ) *err = errno;
		ERRMSG( errno, "Couldn't seek VM address" );
		return 0;
	}
	
	gasp_lseek( handle->wrMemFd, addr, SEEK_SET );
	if ( errno != EXIT_SUCCESS ) {
		if ( err ) *err = errno;
		ERRMSG( errno, "Couldn't seek VM address" );
		return 0;
	}
	
	done = gasp_write( handle->wrMemFd, src, size );
	if ( errno != EXIT_SUCCESS ) {
		if ( err ) *err = errno;
		ERRMSG( errno, "Couldn't override VM" );
		return 0;
	}
	
	gasp_lseek( handle->wrMemFd, off, SEEK_SET );
#endif
	return done;
}
