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

uintmax_t proc_mapped_next(
	int *err, proc_handle_t *handle,
	uintmax_t addr, proc_mapped_t *mapped, mode_t prot
)
{
	int fd;
	char line[PAGE_LINE_SIZE] = {0}, *pos, path[256] = {0};
	
	errno = EXIT_SUCCESS;
	if ( !mapped ) {
		if ( err ) *err = EDESTADDRREQ;
		ERRMSG( EINVAL, "Need destination for page details" );
		return 0;
	}
	
	if ( !handle ) {
		if ( err ) *err = EINVAL;
		ERRMSG( EINVAL, "Invalid handle" );
		return 0;
	}
	
	sprintf( path, "%s/maps", handle->procdir );
	if ( (fd = open( path, O_RDONLY )) < 0 ) {
		if ( err ) *err = errno;
		ERRMSG( errno, "Failed to open maps file" );
		fprintf( stderr, "Path: '%s'\n", path );
		return 0;
	}
	
	do {
		memset( mapped, 0, sizeof(proc_mapped_t) );
		do {
			memset( line, 0, PAGE_LINE_SIZE );
			errno = EXIT_SUCCESS;
			if ( gasp_read( fd, line, PAGE_LINE_SIZE )
				!= PAGE_LINE_SIZE )
				goto done;
			
			line[PAGE_LINE_SIZE-1] = 0;
			sscanf( line, "%p-%p",
				(void**)&(mapped->head),
				(void**)&(mapped->foot) );

			if ( addr < mapped->foot )
				break;
			
			/* Move file pointer to next line */
			while ( *line != '\n' ) {
				/* Ensure we break out when we hit EOF */
				if ( gasp_read( fd, line, 1 ) != 1 )
					goto failed;
			}
		} while ( addr >= mapped->foot );
		
		/* Get position right after address range */
		pos = strchr( line, ' ' );
		/* Capture actual permissions */
		*pos = 0;
		sprintf( path, "%s/map_files/%s", handle->procdir, line );
		mapped->perm = file_glance_perm( err, path );
		/* Move pointer to the permissions text */
		++pos;
		
		mapped->size = (mapped->foot) - (mapped->head);
		mapped->prot |= (pos[0] == 'r') ? 04 : 0;
		mapped->prot |= (pos[1] == 'w') ? 02 : 0;
		mapped->prot |= (pos[2] == 'x') ? 01 : 0;
		/* Check if shared */
		mapped->prot |= (pos[3] == 'p') ? 0 : 010;
		if ( mapped->prot & 010 )
			/* Check if validated share */
			mapped->prot |= (pos[3] == 's') ? 0 : 020;
		
		addr = mapped->foot;
	} while ( (mapped->prot & prot) != prot );
	
	done:
	close(fd);
	return mapped->foot;
	
	failed:
	close(fd);
	//if ( errno == EOF ) errno = EXIT_SUCCESS;
	if ( err ) *err = errno;
	ERRMSG( errno, "Failed to find page that finishes after address" );
	fprintf( stderr, "Wanted: From address %p a page with %c%c%c%c\n",
		(void*)addr,
		((prot & 04) ? 'r' : '-'),
		((prot & 02) ? 'w' : '-'),
		((prot & 01) ? 'x' : '-'),
		((prot & 010) ? ((prot & 020) ? 'v' : 's') : '-')
	);
	return 0;
}

mode_t proc_mapped_addr(
	int *err, proc_handle_t *handle,
	uintmax_t addr, mode_t perm, int *fd, proc_mapped_t *Mapped
)
{
	char path[256] = {0};
	proc_mapped_t mapped = {0};
	
	while ( proc_mapped_next( err, handle, mapped.foot, &mapped, 0 ) ) {
		if ( mapped.foot < addr ) continue;
#ifdef SIZEOF_LLONG
		sprintf( path, "%s/map_files/%llx-%llx",
			handle->procdir,
			(long long)(mapped.head), (long long)(mapped.foot) );
#else
		sprintf( path, "%s/map_files/%lx-%lx",
			handle->procdir,
			(long)(mapped.head), (long)(mapped.foot) );
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

size_t proc__glance_peek(
	int *err, proc_handle_t *handle,
	uintmax_t addr, void *mem, uintmax_t size ) {
	uintmax_t done = 0, dst = 0, predone = 0;
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
	uintmax_t addr, void *mem, uintmax_t size ) {
#ifdef _GNU_SOURCE
	struct iovec internal, external;
	uintmax_t done, predone = 0;
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

uintmax_t proc__glance_file(
	int *err, proc_handle_t *handle,
	uintmax_t addr, void *mem, size_t size
)
{
#ifdef gasp_pread
	intmax_t done = 0;
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

uintmax_t proc__glance_self(
	int *err, proc_handle_t *handle,
	uintmax_t addr, void *mem, size_t size
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

uintmax_t proc__glance_seek(
	int *err, proc_handle_t *handle,
	uintmax_t addr, void *mem, size_t size
)
{
	char path[SIZEOF_PROCDIR+5] = {0};
	uintmax_t done = 0;
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

size_t proc__change_poke(
	int *err, proc_handle_t *handle,
	uintmax_t addr, void *mem, uintmax_t size ) {
	uintmax_t done = 0, dst = 0;
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
	uintmax_t addr, void *mem, uintmax_t size ) {
#ifdef _GNU_SOURCE
	struct iovec internal, external;
	uintmax_t done;
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

uintmax_t proc__change_file(
	int *err, proc_handle_t *handle,
	uintmax_t addr, void *mem, size_t size
)
{
#ifdef gasp_pwrite
	intmax_t done = 0;
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
	uintmax_t addr, void *mem, size_t size
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

uintmax_t proc__change_seek(
	int *err, proc_handle_t *handle,
	uintmax_t addr, void *mem, size_t size
)
{
	char path[SIZEOF_PROCDIR+5] = {0};
	uintmax_t done = 0;
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

int gasp_isdir( char const *path ) {
	struct stat st = {0};
	if ( !path )
		return EINVAL;
	if ( stat( path, &st ) != 0 )
	{
		if ( errno == ENOENT )
			errno = EXIT_FAILURE;
		return errno;
	}
	if ( !S_ISDIR(st.st_mode) )
		return EXIT_FAILURE;
	return EXIT_SUCCESS;
}

int gasp_mkdir( space_t *space, char const *path ) {
	int ret = gasp_isdir( path );
	char *temp;
	if ( ret != EXIT_FAILURE )
		return ret;
	if ( !space )
		return EDESTADDRREQ;
	if ( !(temp = more_space( &ret, space, strlen( path ) + 9 )) )
		return ret;
	(void)sprintf( temp, "mkdir \"%s\"\n", path );
	fprintf( stderr, "%s\n", temp );
	return system( temp );
}

int gasp_rmdir( space_t *space, char const *path, bool recursive ) {
	int ret = gasp_isdir( path );
	char *temp;
	if ( ret == EXIT_FAILURE )
		return EXIT_SUCCESS;
	if ( ret != EXIT_SUCCESS )
		return ret;
	if ( !space )
		return EDESTADDRREQ;
	if ( !(temp = more_space( &ret, space, strlen( path ) + 9 )) )
		return ret;
	(void)sprintf( temp, "rm -%c \"%s\"",
		recursive ? 'r' : 'd', path );
	return system( temp );
}

int gasp_open( int *fd, char const *path, int perm )
{
	int ret = EXIT_SUCCESS;
	int with = ((perm & 0600) == 0600) ? O_RDWR : 0;
	if ( !fd ) return EDESTADDRREQ;
	if ( !path ) return EINVAL;
	if ( !with ) {
		with |= ((perm & 0400) == 0400) ? O_RDONLY : 0;
		with |= ((perm & 0200) == 0200) ? O_WRONLY : 0;
	}
	//with |= ((perm & 0100) == 0100) ? O_EXEC : 0;
	if ( (*fd = open( path, with )) < 0 ) {
		if ( errno == ENOENT ) {
			if ( (*fd =	open( path, O_CREAT | with, perm )) < 0 )
				return errno;
		}
		else return errno;
	}
	return ret;
}

int open_dump_files( dump_t *dump, long inst, long done ) {
	int ret = EXIT_SUCCESS;
	char *path;
	char const *GASP_PATH = getenv("GASP_PATH");
	size_t len = strlen(GASP_PATH) + (2 * (sizeof(node_t) * CHAR_BIT));
	space_t space = {0}, path_space = {0};
	if ( !dump ) return EDESTADDRREQ;
	if ( !(path = more_space( &ret, &path_space, len )) )
		return ret;
	if ( (ret = gasp_mkdir( &space, GASP_PATH )) != EXIT_SUCCESS )
		goto fail;
	sprintf( path, "%s/scans", GASP_PATH );
	if ( (ret = gasp_mkdir( &space, path )) != EXIT_SUCCESS )
		goto fail;
	sprintf( path, "%s/scans/%ld", GASP_PATH, inst );
	if ( (ret = gasp_mkdir( &space, path )) != EXIT_SUCCESS )
		goto fail;
	sprintf( path, "%s/scans/%ld/%ld.info", GASP_PATH, inst, done );
	if ( (ret = gasp_open( &(dump->info_fd), path, 0700 ))
		!= EXIT_SUCCESS )
		goto fail;
	sprintf( path, "%s/scans/%ld/%ld.used", GASP_PATH, inst, done );
	if ( (ret = gasp_open( &(dump->used_fd), path, 0700 ))
		!= EXIT_SUCCESS )
		goto fail;
	sprintf( path, "%s/scans/%ld/%ld.data", GASP_PATH, inst, done );
	ret = gasp_open( &(dump->data_fd), path, 0700 );
	fail:
	free_space( NULL, &path_space );
	free_space( NULL, &space );
	return ret;
}

void shut_dump_files( dump_t *dump ) {
	if ( dump->info_fd >= 0 ) close( dump->info_fd );
	if ( dump->used_fd >= 0 ) close( dump->used_fd );
	if ( dump->data_fd >= 0 ) close( dump->data_fd );
	memset( dump->used, 0, DUMP_MAX_SIZE );
	memset( dump->data, 0, DUMP_MAX_SIZE );
	memset( dump, 0, sizeof(dump_t) );
	memset( dump, 0, sizeof(dump_t) );
	dump->info_fd = dump->used_fd = dump->data_fd = -1;
}

int check_dump( dump_t dump ) {
	if ( dump.info_fd <= 0 || dump.used_fd <= 0 || dump.data_fd <= 0 )
	{
		fprintf( stderr, "info_fd = %d, used_fd = %d, data_fd = %d\n",
			dump.info_fd, dump.used_fd, dump.data_fd );
		return EBADF;
	}
	return EXIT_SUCCESS;
}

void shut_dump( dump_t *dump ) {
	if ( dump->info_fd > 0 )
		close( dump->info_fd );
	if ( dump->used_fd > 0 )
		close( dump->used_fd );
	if ( dump->data_fd > 0 )
		close( dump->data_fd );
	dump->info_fd = dump->used_fd = dump->data_fd = -1;
}

uintmax_t proc_handle_dump_nextbyproc(
	int *err, proc_handle_t *handle, dump_t dump, proc_mapped_t *mapped
)
{
	(void)dump;
	return proc_mapped_next( err, handle, mapped->foot, mapped, 06 );
}
uintmax_t proc_handle_dump_nextbyfile(
	int *err, proc_handle_t *handle, dump_t dump, proc_mapped_t *mapped
)
{
	(void)handle;
	if ( read( dump.info_fd, mapped, sizeof(proc_mapped_t) )
		!= sizeof(proc_mapped_t) && err )
		*err = errno;
	return mapped->foot;
}

typedef uintmax_t (*func_proc_handle_dump_next)(
	int *err, proc_handle_t *handle, dump_t dump, proc_mapped_t *mapped
);

typedef uintmax_t (*func_proc_handle_rdwr_t)(
	int *err, proc_handle_t *handle,
		uintmax_t addr, void *buff, uintmax_t size
);

bool gasp_tpolli( int int_pipes[2], int *sig, int msTimeout )
{
	struct pollfd pfd = {0};
	pfd.fd = int_pipes[0];
	pfd.events = POLLIN;
	if ( poll( &pfd, 1, msTimeout ) != 1 )
		return 0;
	read( int_pipes[0], sig, sizeof(int) );
	return 1;
}

bool gasp_tpollo( int ext_pipes[2], int sig, int msTimeout )
{
	struct pollfd pfd = {0};
	pfd.fd = ext_pipes[1];
	pfd.events = POLLOUT;
	if ( poll( &pfd, 1, msTimeout ) <= 0 )
		return 0;
	write( ext_pipes[1], &sig, sizeof(int) );
	return 1;
}

bool gasp_thread_should_die( int ext_pipes[2], int int_pipes[2] )
{
	int ret = SIGCONT;
	if ( !gasp_tpolli( int_pipes, &ret, 1 ) )
		return 0;
	switch ( ret )
	{
	case SIGKILL:
		return 1;
	case SIGCONT:
	case SIGSTOP:
		if ( !gasp_tpollo( ext_pipes, SIGCONT, -1 ) )
			return 1;
		if ( !gasp_tpolli( int_pipes, &ret, -1 ) )
			return 1;
		return (ret != SIGCONT);
	}
	return 0;
}

node_t proc_handle_dump( tscan_t *tscan ) {
	int ret;
	node_t n = 0;
	proc_mapped_t mapped = {0};
	proc_handle_t *handle;
	uchar *used, *data;
	dump_t *dump;
	uintmax_t i, bytes;
	ssize_t size;
	mode_t prot = 04;
	func_proc_handle_dump_next next_mapped;
	
	if ( !tscan ) return 0;
	
	dump = &(tscan->dump[1]);
	used = dump->used;
	data = dump->data;
	tscan->last_found = 0;
	prot |= tscan->writeable ? 02 : 0;
	handle = tscan->handle;
	next_mapped =
		tscan->done_scans
			? proc_handle_dump_nextbyfile
			: proc_handle_dump_nextbyproc;
	
	if ( !(tscan->upto) || tscan->upto < tscan->from )
		tscan->upto = UINTMAX_C(~0);
	
	if ( !handle ) {
		tscan->ret = EINVAL;
		return 0;
	}
	
	if ( (ret = check_dump( *dump )) != EXIT_SUCCESS )
		goto done;
	
	if ( tscan->zero != 0 )
		tscan->zero = ~0;
	
	if ( gasp_lseek( dump->info_fd, 0, SEEK_SET ) != 0 )
	{
		ret = errno;
		goto done;
	}
	
	if ( gasp_lseek( dump->used_fd, 0, SEEK_SET ) != 0 )
	{
		ret = errno;
		goto done;
	}
	
	if ( gasp_lseek( dump->data_fd, 0, SEEK_SET ) != 0 )
	{
		ret = errno;
		goto done;
	}
	
	/* Set Scan/Dump Number */
	if ( gasp_write( dump->info_fd, &n, sizeof(node_t) )
		!= sizeof(node_t) )
	{
		ret = errno;
		goto done;
	}
	
	/* Initialise mapped count */
	n = 0;
	if ( gasp_write( dump->info_fd, &n, sizeof(node_t) )
		!= sizeof(node_t) )
	{
		ret = errno;
		goto done;
	}
	
	/* Since this is a dump set all address booleans to true,
	 * a later scan can simply override the values,
	 * we use ~0 now because it's bit scan friendly,
	 * not that we're doing those yet */
	(void)memset( used, ~0, DUMP_MAX_SIZE );
	
	while ( next_mapped( &ret, handle, *dump, &mapped )  )
	{
		tscan->done_upto = mapped.head;
		/* Not interested in unreadable memory */
		if ( (mapped.prot & prot) != prot )
			continue;
		
		/* Not interested in out of range memory */
		if ( mapped.foot <= tscan->from || mapped.head >= tscan->upto )
			continue;
		
		for (
			bytes = 0;
			bytes < mapped.size;
			bytes += i
		)
		{
			i = mapped.size - bytes;
			if ( i > DUMP_MAX_SIZE ) i = DUMP_MAX_SIZE;
			
			if ( gasp_thread_should_die(
				tscan->main_pipes, tscan->scan_pipes )
			)
			{
				tscan->ret = EXIT_FAILURE;
				return 0;
			}
			
			/* Prevent unread memory from corrupting locations found */
			(void)memset( data, tscan->zero, i );
			
			if ( !(size = proc_glance_data(
				NULL, handle, mapped.head, data, i )) )
				break;
			
			if ( gasp_write( dump->data_fd, data, size ) != size )
			{
				ret = errno;
				goto done;
			}
		
			if ( gasp_write( dump->used_fd, used, size ) != size )
			{
				ret = errno;
				goto done;
			}
			
			fsync( dump->used_fd );
			fsync( dump->data_fd );
		}
		
		if ( bytes == mapped.size ) {			
			/* Add the mapping to the list */
			if ( gasp_write(
				dump->info_fd, &mapped, sizeof(proc_mapped_t) )
				!= (ssize_t)sizeof(proc_mapped_t) )
			{
				ret = errno;
				goto done;
			}
			++n;
		}
	}
	
	if ( gasp_lseek( dump->info_fd, 0, SEEK_SET ) != 0 ) {
		ret = errno;
		goto done;
	}
	
	if ( gasp_write( dump->info_fd, &n, sizeof(node_t) )
		!= sizeof(node_t) )
		ret = errno;
	
	done:
	fsync( dump->info_fd );
	tscan->ret = ret;
	tscan->last_found = n;
	return n;
}

bool glance_dump( int *err, dump_t *dump, bool *both, size_t keep )
{
	int ret = EXIT_SUCCESS;
	ssize_t size = 0;
	size_t pos = DUMP_BUF_SIZE - keep;
	uchar *data, *used;
	proc_mapped_t *pmap, *nmap;
	uintmax_t addr;
	
	if ( !dump )
	{
		ret = EINVAL;
		goto failure;
	}
	
	/* Initialise pointers */
	pmap = dump->pmap;
	nmap = dump->nmap;
	used = dump->used;
	data = dump->data;
	
	/* Swap info when changing region */
	if ( dump->done == nmap->size )
	{
		if ( nmap > pmap )
		{
			nmap = pmap;
			pmap = nmap + 1;
		}
		else
		{
			pmap = nmap;
			nmap = pmap + 1;
		}
		dump->pmap = pmap;
		dump->nmap = nmap;
		dump->done = 0;
	}
	
	/* Ensure invalid region handed back on failure */
	*both = 1;
	memset( nmap, 0 , sizeof(proc_mapped_t) );
	
	if ( gasp_read( dump->info_fd, nmap, sizeof(proc_mapped_t) )
		!= sizeof(proc_mapped_t) )
	{
		if ( errno == EXIT_SUCCESS ) errno = EIO;
		ret = errno;
		ERRMSG( ret, "Failed to read info file" );
		goto failure;
	}
	
	if ( !(nmap->size) ) {
		ret = EINVAL;
		ERRMSG( ret, "Invalid region size recorded" );
		goto failure;
	}
	
	size = nmap->size - dump->done;
	if ( size > DUMP_BUF_SIZE ) size = DUMP_BUF_SIZE;
	
	if ( gasp_read( dump->used_fd,
			used + DUMP_BUF_SIZE, size ) != size )
	{
		if ( errno == EXIT_SUCCESS ) errno = EIO;
		ret = errno;
		ERRMSG( ret, "Failed to read used addresses file" );
		goto failure;
	}
	if ( gasp_read( dump->data_fd,
		data + DUMP_BUF_SIZE, size ) != size )
	{
		if ( errno == EXIT_SUCCESS ) errno = EIO;
		ret = errno;
		ERRMSG( ret, "Failed to read dump file" );
		goto failure;
	}
	
	addr = nmap->head;
	dump->size = size;
	if ( pos < DUMP_BUF_SIZE )
	{
		if ( dump->done || nmap->head == pmap->foot )
		{
			addr = dump->addr;
			dump->size = size + keep;
			(void)memmove( used, used + pos, dump->size );
			(void)memmove( data, data + pos, dump->size );
		}
	}
	dump->addr = addr;
	
	return 1;
	
	failure:
	if ( err ) *err = ret;
	if ( ret == ENOMEM ) {
		fprintf( stderr, "Requested 0x%08jX bytes\n", size );
	}
	return 0;
}

bool change_dump(
	dump_t *dump,
	bool both,
	size_t keep
)
{
	uchar *data, *used;
	ssize_t pos;
	size_t size;
	
	if ( !dump ) return 0;
	
	size = dump->size;
	used = dump->used;
	data = dump->data;
	
	if ( both )
	{
		if ( !keep || keep >= DUMP_BUF_SIZE ) return ERANGE;
		pos = keep;
		size = DUMP_BUF_SIZE + keep;
		gasp_lseek( dump->used_fd, -pos, SEEK_CUR );
		gasp_lseek( dump->data_fd, -pos, SEEK_CUR );
		gasp_write( dump->used_fd, used, size );
		gasp_write( dump->data_fd, data, size );
	}
	else
	{
		gasp_write( dump->used_fd, used, size );
		gasp_write( dump->data_fd, data, size );
	}
	
	fsync( dump->used_fd );
	fsync( dump->data_fd );
	return 1;
}

bool proc_handle__aobscan_next(
	tscan_t *tscan, dump_t *dump, bool *both )
{
	int ret = 0;
	if ( !tscan ) return 0;
	if ( gasp_thread_should_die(
		tscan->main_pipes, tscan->scan_pipes ) )
	{
		tscan->ret = EXIT_FAILURE;
		return 0;
	}
	return glance_dump( &ret, dump, both, tscan->bytes - 1 );
}

bool proc_handle__aobscan_test( tscan_t *tscan, bool both )
{
	uchar *used, *data;
	uintmax_t addr, minus, a, stop;
	dump_t *dump;
	if ( !tscan ) return 0;
	if ( gasp_thread_should_die(
		tscan->main_pipes, tscan->scan_pipes )
	)
	{
		tscan->ret = EXIT_FAILURE;
		return 0;
	}
	minus = tscan->bytes - 1;
	dump = &(tscan->dump[1]);
	used = dump->used;
	data = dump->data;
	addr = dump->addr;
	stop = dump->size - tscan->bytes;
	for ( a = 0; a < stop; ++a, ++addr )
	{
		if ( used[a] ) {
			used[a] = (
				memcmp( data + a, tscan->array, tscan->bytes ) == 0
			);
			if ( used[a] )
				tscan->found++;
		}
		tscan->done_upto = addr;
	}
	both = change_dump( dump, both, minus );
	return both;
}

void* proc_handle_aobscan( tscan_t *tscan ) {
	uchar *buff = NULL, *used;
	uintmax_t stop = 0, minus;
	mode_t prot = 04;
	node_t a, number = 0, regions = 0;
	bool pboth = 0, both = 0;
	
	if ( !tscan ) {
		errno = EDESTADDRREQ;
		ERRMSG( errno, "bytescanner was given NULL" );
		return NULL;
	}
	
	minus = tscan->bytes - 1;
	tscan->found = 0;
	tscan->done_upto = 0;
	
	if ( minus > tscan->bytes ) {
		tscan->ret = EINVAL;
		goto set_found;
	}

	if ( (tscan->ret = errno = check_dump( tscan->dump[1] ))
		!= EXIT_SUCCESS )
		goto set_found;

	prot |= tscan->writeable ? 02 : 0;
	
	if ( (tscan->ret =
		proc__rwvmem_test(tscan->handle, tscan->array, tscan->bytes ))
		!= EXIT_SUCCESS ){
		fprintf( stderr, "Failed at proc__rwvmen_test()\n" );
		return tscan;
	}
	
	if ( gasp_lseek( tscan->dump[1].used_fd, 0, SEEK_SET ) < 0 ) {
		fprintf( stderr, "Failed at lseek(used_fd)\n" );
		tscan->ret = errno;
		goto set_found;
	}
	if ( gasp_lseek( tscan->dump[1].data_fd, 0, SEEK_SET ) < 0 ) {
		fprintf( stderr, "Failed at lseek(data_fd)\n" );
		tscan->ret = errno;
		goto set_found;
	}
	
	/* Reduce corrupted results */
	(void)memset( tscan->dump[0].pmap, 0, sizeof(proc_mapped_t) );
	(void)memset( tscan->dump[0].nmap, 0, sizeof(proc_mapped_t) );
	(void)memset( tscan->dump[1].pmap, 0, sizeof(proc_mapped_t) );
	(void)memset( tscan->dump[1].nmap, 0, sizeof(proc_mapped_t) );

	/* Get file position correct before we start each instance of
	 * proc_mapped_t object to file */
	write( tscan->dump[1].info_fd, &number, sizeof(node_t) );
	write( tscan->dump[1].info_fd, &regions, sizeof(node_t) );
	if ( !(tscan->done_scans) ) {
		while (
			regions-- &&
			proc_handle__aobscan_next( tscan, tscan->dump + 1, &both )
		)
		{
			fprintf( stderr, "%p-%p\n",
				(void*)(tscan->dump[1].nmap->head),
				(void*)(tscan->dump[1].nmap->foot)
			);
			
			used = tscan->dump[1].used;
			/* Ensure we check all addresses */
			for ( a = 0; a < stop; ++a ) used[a] = ~0;
			if ( !proc_handle__aobscan_test( tscan, both ) )
				goto set_found;
			
		}
	}
	else
	{
		/* Reset pointer */
		if ( gasp_lseek( tscan->dump[0].info_fd, 0, SEEK_SET ) < 0 ) {
			tscan->ret = errno;
			goto set_found;
		}
		
		/* Align pointer to where the proc_mapped_t objects begin */
		read( tscan->dump[0].info_fd, &number, sizeof(node_t) );
		read( tscan->dump[0].info_fd, &regions, sizeof(node_t) );
		
		/* Make sure we're are not attempting to compare against
		 * incorrect results */
		if ( number != tscan->done_scans )
			goto set_found;
		if ( gasp_lseek( tscan->dump[0].used_fd, 0, SEEK_SET ) < 0 ) {
			tscan->ret = errno;
			goto set_found;
		}
		if ( gasp_lseek( tscan->dump[0].data_fd, 0, SEEK_SET ) < 0 ) {
			tscan->ret = errno;
			goto set_found;
		}
		while ( regions-- &&
			proc_handle__aobscan_next( tscan, tscan->dump, &pboth )
		)
		{
			if ( !proc_handle__aobscan_next(
				tscan, tscan->dump + 1, &both ) || pboth != both )
				goto set_found;
			buff = tscan->dump[0].used;
			used = tscan->dump[1].used;
			/* Ensure we check all addresses */
			for ( a = 0; a < stop; ++a ) used[a] = buff[a];
			if ( !proc_handle__aobscan_test( tscan, both ) )
				goto set_found;
		}
	}
	tscan->ret = EXIT_SUCCESS;
	tscan->done_upto = tscan->upto;
	set_found:
	
	tscan->last_found = tscan->found;
	tscan->done_scans++;
	
	if ( tscan->ret != EXIT_SUCCESS )
		ERRMSG( tscan->ret, "Byte scan failed to run fully" );
	return tscan;
}

void* proc_handle_dumper( void * _tscan ) {
	tscan_t *tscan = _tscan;
	
	if ( !tscan ) return NULL;
	tscan->threadmade = 1;
	tscan->found = 0;
	
	tscan->dumping = 1;
	(void)proc_handle_dump( tscan );
	tscan->dumping = 0;
	
	tscan->scanning = 1;
	(void)proc_handle_aobscan( tscan );
	tscan->scanning = 0;
	
	gasp_tpollo( tscan->main_pipes, SIGCONT, 0 );
	tscan->threadmade = 0;
	return _tscan;
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
	intmax_t size;
	
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

uintmax_t proc_glance_data(
	int *err, proc_handle_t *handle,
	uintmax_t addr, void *mem, size_t size
)
{
	uintmax_t done;
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

uintmax_t proc_change_data(
	int *err, proc_handle_t *handle,
	uintmax_t addr, void *mem, size_t size
)
{
	uintmax_t done;
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
