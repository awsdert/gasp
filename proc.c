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
		errno = 0;
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
	int ret = 0;
	mode_t was = file_glance_perm( &ret, path );
	if ( !was && ret != 0 ) {
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
	
	errno = 0;
	
	if ( stat( path, &st ) == 0 && st.st_size > 0 )
		return st.st_size;
	
	errno = 0;
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

int proc_mapped_list( proc_handle_t *handle, nodes_t *nodes )
{
	int ret = 0, fd = -1;
	proc_mapped_t m = {0}, *mv = NULL;
	char line[PAGE_LINE_SIZE] = {0}, *pos, path[SCHAR_MAX] = {0};
	
	if ( !handle )
	{
		ret = EINVAL;
		ERRMSG( ret, "Must have a handle to identify mappings" );
		return ret;
	}
	
	if ( !nodes )
	{
		ret = EDESTADDRREQ;
		ERRMSG( ret, "Must have somewhere to record mappings to" );
		return ret;
	}
		
	nodes->count = nodes->total = 0;
	
	(void)sprintf( path, "%s/maps", handle->procdir );
	if ( (fd = open( path, O_RDONLY )) < 0 )
	{
		ret = errno;
		ERRMSG( ret, "Couldn't open '/proc/<PID>/maps' file" );
		return ret;
	}
	
	for (
		; gasp_read( fd, line, PAGE_LINE_SIZE ) == PAGE_LINE_SIZE
		; (void)memset( line, 0, PAGE_LINE_SIZE )
	)
	{
		(void)memset( &m, 0, sizeof(proc_mapped_t) );
		
		/* Read address range */
		sscanf( line, "%jx-%jx", &(m.head), &(m.foot) );
		
		/* Get position right after address range */
		pos = strchr( line, ' ' );
		
		/* Move pointer to the permissions text */
		++pos;
		
		sprintf( path, "%s/map_files/%s", handle->procdir, line );
		m.perm = file_glance_perm( &ret, path );
		
		m.size = (m.foot) - (m.head);
		m.prot |= (pos[0] == 'r') ? 04 : 0;
		m.prot |= (pos[1] == 'w') ? 02 : 0;
		m.prot |= (pos[2] == 'x') ? 01 : 0;
		/* Check if shared */
		m.prot |= (pos[3] == 'p') ? 0 : 010;
		if ( m.prot & 010 )
			/* Check if validated share */
			m.prot |= (pos[3] == 's') ? 0 : 020;
			
		
		if ( nodes->count == nodes->total && !(mv = more_nodes(
			proc_mapped_t, &ret, nodes, nodes->total + SCHAR_MAX ))
		)
		{
			return ret;
		}
		
		mv[nodes->count] = m;
		nodes->count++;
	}
	
	ret = errno;
	close(fd);
	return ret;
}

int proc_mapped_next(
	proc_handle_t *handle, mode_t prot, proc_mapped_t *mapped
)
{
	int ret = 0;
	nodes_t nodes = {0};
	node_t n;
	proc_mapped_t *m, *mv;
	
	if ( !mapped ) {
		ret = EDESTADDRREQ;
		ERRMSG( ret, "Need destination for page details" );
		return ret;
	}
	
	if ( (ret = proc_mapped_list( handle, &nodes )) != 0 )
		return ret;
	
	mv = nodes.space.block;
	
	for ( n = 0; n < nodes.count; ++n ) {
		m = mv + n;
		if ( mapped->foot <= m->foot || (m->prot & prot) != prot )
			continue;
		
		(void)memcpy( mapped, m, sizeof(proc_mapped_t) );
		break;
	}
	
	if ( n == nodes.count )
	{
		(void)memset( mapped, 0, sizeof(proc_mapped_t) );
		ret = ERANGE;
	}
	
	free_nodes( proc_mapped_t, NULL, &nodes );
	
	return ret;
}

int proc_mapped_addr(
	proc_handle_t *handle, proc_mapped_t *mapped, uintmax_t addr
)
{
	int ret = 0;
	nodes_t nodes = {0};
	node_t n;
	proc_mapped_t *m, *mv;
	
	if ( !mapped ) {
		ret = EDESTADDRREQ;
		ERRMSG( ret, "Need destination for mapping details" );
		return ret;
	}
	
	if ( (ret = proc_mapped_list( handle, &nodes )) != 0 )
		return ret;
	
	mv = nodes.space.block;
	
	for ( n = 0; n < nodes.count; ++n ) {
		m = mv + n;
		if ( addr < m->head || addr > m->foot )
			continue;
		
		(void)memcpy( mapped, m, sizeof(proc_mapped_t) );
		break;
	}
	
	if ( n == nodes.count )
	{
		(void)memset( mapped, 0, sizeof(proc_mapped_t) );
		ret = ERANGE;
	}
	
	free_nodes( proc_mapped_t, NULL, &nodes );
	
	return ret;
}

int proc__rwvmem_test(
	proc_handle_t *handle, void *mem, size_t size
)
{
	int ret = 0;
	
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
	
	if ( (ret = proc__rwvmem_test(handle, mem,size)) != 0 )
	{
		if ( err ) *err = ret;
		return 0;
	}

	errno = 0;
	
	for ( temp = 0; addr % sizeof(long); ++temp, --addr );
	if ( temp ) {
		dst = ptrace( PTRACE_PEEKDATA,
			handle->notice.entryId, (void*)addr, NULL );
		if ( errno != 0 )
			return 0;
		temp = (sizeof(long) - (predone = temp));
		memcpy( m, (&dst) + predone, temp );
		addr += sizeof(long);
		size -= predone;
	}
	
	while ( done < size ) {
		dst = ptrace( PTRACE_PEEKTEXT,
			handle->notice.entryId, (void*)(addr + done), NULL );
		if ( errno != 0 )
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
	
	if ( (ret = proc__rwvmem_test(handle, mem,size)) != 0 )
	{
		if ( err ) *err = ret;
		return 0;
	}

	errno = 0;
#if 0
	if ( addr % 0xFF ) {
		predone = proc__glance_peek( &ret, handle, addr, mem,
			0xFF - (addr % 0xFF) );
		if ( ret != 0 ) {
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
	
	if ( (ret = proc__rwvmem_test(handle, mem,size)) != 0 )
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
	if ( handle->memfd <= 0 )
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
	
	if ( (ret = proc__rwvmem_test(handle, mem,size)) != 0 )
	{
		if ( err ) *err = ret;
		return 0;
	}
	
	if ( handle->notice.self ) {
		(void)memmove( mem, (void*)addr, size );
		if ( err ) *err = errno;
		if ( errno != 0 )
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
	
	if ( (ret = proc__rwvmem_test(handle, mem,size)) != 0 )
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
	if ( errno != 0 )
		goto failed;
	
	done = gasp_read( fd, mem, size );
	if ( done <= 0 || errno != 0 )
		goto failed;

	if ( handle->memfd <= 0 )
		close(fd);
	return done;
	
	failed:
	if ( handle->memfd <= 0 )
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
	
	if ( (ret = proc__rwvmem_test(handle, mem,size)) != 0 )
	{
		if ( err ) *err = ret;
		return 0;
	}

	errno = 0;
	
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
	
	if ( (ret = proc__rwvmem_test(handle, mem,size)) != 0 )
	{
		if ( err ) *err = ret;
		return 0;
	}

	errno = 0;
	
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
	if ( (ret = proc__rwvmem_test(handle, mem,size)) != 0 )
	{
		if ( err ) *err = ret;
		return 0;
	}
	
	if ( handle->perm != O_RDWR || handle->memfd <= 0 )
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
	
	if ( (ret = proc__rwvmem_test(handle, mem,size)) != 0 )
	{
		if ( err ) *err = ret;
		return 0;
	}
	
	if ( handle->notice.self ) {
		errno = 0;
		(void)memmove( (void*)addr, mem, size );
		if ( errno != 0 ) {
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
	
	if ( (ret = proc__rwvmem_test(handle, mem,size)) != 0 )
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
	if ( errno != 0 )
		goto failed;
	
	done = gasp_write( fd, mem, size );
	if ( done <= 0 || errno != 0 )
		goto failed;

	close(fd);
	return done;
	
	failed:
	close(fd);
	if ( err ) *err = errno;
	ERRMSG( errno, "Couldn't write to process memory" );
	return 0;
}

void gasp_close_pipes( int *pipes ) {
	int pd, i;
	for ( i = 1; i >= 0; --i ) {
		/* Store the pipe for usage later */
		pd = pipes[i];
		/* Force poll, write etc to just error out with EBADF */
		pipes[i] = -1;
		/* Actually close the pipe */
		if ( pd > 0 ) close( pd );
	}
}

int gasp_isdir( char const *path, bool make ) {
	struct stat st = {0};
	if ( !path )
		return EINVAL;
	if ( stat( path, &st ) != 0 )
	{
		if ( errno == ENOTDIR || errno == ENOENT ) {
			if ( make ) {
				errno = 0;
				(void)mkdir(path,0755);
				return errno;
			}
			errno = ENOTDIR;
		}
		return errno;
	}
	if ( !S_ISDIR(st.st_mode) )
		return EXIT_FAILURE;
	return 0;
}

int gasp_rmdir( space_t *space, char const *path, bool recursive ) {
	int ret = gasp_isdir( path, 0 );
	char *temp;
	if ( ret == ENOENT )
		return 0;
	if ( ret != 0 )
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
	int ret = 0;
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

void dump_files__init(
	dump_file_t *dump_file, void *data, size_t size
)
{
	
	if ( !dump_file )
		return;
	
	/* Clear buffer if exists */
	if ( data )
		(void)memset( data, 0, size );
	
	/* Clear whole object */
	(void)memset( dump_file, 0, sizeof(dump_file_t) );
	
	/* Set pointer and size */
	dump_file->data = data;
	dump_file->size = size;
}

void dump_files__shut_file( dump_file_t *dump_file )
{
	if ( !dump_file )
		return;
	
	/* Close file descriptor */
	if ( dump_file->fd > 0 )
		close( dump_file->fd );
	
	dump_files__init( dump_file, dump_file->data, dump_file->size );
}

int dump_files__open_file(
	dump_file_t *dump_file,
	space_t *space, long inst, long scan,
	char const *ext
)
{
	int ret = 0;
	char const *GASP_PATH;
	char *path;
	size_t len;
	
	if ( !dump_file || !space || !ext ) return EDESTADDRREQ;
	dump_files__init( dump_file, dump_file->data, dump_file->size );
	
	GASP_PATH = getenv("GASP_PATH");
	len = strlen(GASP_PATH) + (2 * (sizeof(node_t) * CHAR_BIT));
	if ( !(path = more_space( &ret, space, len )) )
		return ret;

	sprintf( path, "%s/scans/%ld/%ld%s", GASP_PATH, inst, scan, ext );
	return gasp_open( &(dump_file->fd), path, 0700 );
}

int dump_files__read_file(
	dump_file_t *dump_file, void *dst, ssize_t size, ssize_t *done
)
{
	if ( !dump_file || !dst || size < 1 || !done )
		return EINVAL;

	errno = 0;
	if ( (dump_file->prv = gasp_lseek( dump_file->fd, 0, SEEK_CUR ))
		< 0
	)
	{
		if ( errno == 0 ) errno = EIO;
		ERRMSG( errno, "Failed to glance at file offset" );
		return errno;
	}
	
	errno = 0;
	if ( (*done = gasp_read( dump_file->fd, dst, size )) < 0 )
	{
		if ( errno == 0 ) errno = EIO;
		ERRMSG( errno, "Failed to glance at file data" );
		return errno;
	}
	
	errno = 0;
	if ( (dump_file->pos = gasp_lseek( dump_file->fd, 0, SEEK_CUR ))
		< 0
	)
	{
		if ( errno == 0 ) errno = EIO;
		ERRMSG( errno, "Failed to glance at file offset" );
		return errno;
	}
	return 0;
}

int dump_files__write_file(
	dump_file_t *dump_file, void *dst, ssize_t size, ssize_t *done
)
{
	if ( !dump_file || !dst || size < 1 || !done )
		return EINVAL;

	errno = 0;
	if ( (dump_file->prv = gasp_lseek( dump_file->fd, 0, SEEK_CUR ))
		< 0
	)
	{
		if ( errno == 0 ) errno = EIO;
		ERRMSG( errno, "Failed to glance at file offset" );
		return errno;
	}
	
	errno = 0;
	if ( (*done = gasp_write( dump_file->fd, dst, size )) < 0 )
	{
		if ( errno == 0 ) errno = EIO;
		ERRMSG( errno, "Failed to glance at file data" );
		return errno;
	}
	
	errno = 0;
	if ( (dump_file->pos = gasp_lseek( dump_file->fd, 0, SEEK_CUR ))
		< 0
	)
	{
		if ( errno == 0 ) errno = EIO;
		ERRMSG( errno, "Failed to glance at file offset" );
		return errno;
	}
	return 0;
}

int dump_files__dir( space_t *space, long inst, bool make ) {
	int ret = 0;
	char const *GASP_PATH;
	char *path;
	size_t len;
	
	if ( !space )
		return EDESTADDRREQ;
	GASP_PATH = getenv("GASP_PATH");
	if ( !GASP_PATH )
		return EINVAL;
	len = strlen(GASP_PATH) + (2 * (sizeof(node_t) * CHAR_BIT));
	
	if ( !(path = more_space( &ret, space, len )) )
		return ret;
	
	if ( (ret = gasp_isdir( GASP_PATH, make )) != 0 )
		return ret;
	
	sprintf( path, "%s/scans", GASP_PATH );
	if ( (ret = gasp_isdir( path, make )) != 0 )
		return ret;
	
	sprintf( path, "%s/scans/%ld", GASP_PATH, inst );
	if ( (ret = gasp_isdir( path, make )) != 0 )
		return ret;
	
	return 0;
}

int dump_files_init( dump_t *dump ) {
	uint i;
	uchar *_;
	
	if ( !dump )
		return EDESTADDRREQ;
		
	(void)memset( dump, 0, sizeof( dump_t ) );
	
	_ = dump->_;
	for ( i = 0; i < DUMP_LOC_NODE_UPTO; ++i ) {
		dump->__[i] = _ + (i * DUMP_LOC_NODE_SIZE);
	}
	
	dump->info.fd = dump->used.fd = dump->data.fd = -1;
	
	dump->info.data = dump->nodes.space.block;
	dump->used.data = dump->__[DUMP_LOC_NODE_USED];
	dump->data.data = dump->__[DUMP_LOC_NODE_DATA];
	
	dump->info.size = dump->nodes.space.given;
	dump->used.size = DUMP_LOC_NODE_SIZE;
	dump->data.size = DUMP_LOC_NODE_SIZE;
	
	return 0;
}

int dump_files_open( dump_t *dump, long inst, long scan ) {
	int ret = 0;
	space_t space = {0};
	
	if ( !dump )
		return EDESTADDRREQ;
	
	/* Cleanse structure of left over data data */
	if ( dump_files_test( *dump ) == 0 )
		dump_files_shut( dump );
	else
		(void)dump_files_init( dump );
	
	if ( (ret = dump_files__dir( &space, inst, 1 ))
		!= 0 ) {
		ERRMSG( ret, "Couldn't create needed directories" );
		goto fail;
	}

	if ( (ret =
		dump_files__open_file(
			&(dump->info), &space, inst, scan, ".info" )) != 0 )
	{
		ERRMSG( ret,
			"Couldn't create/open %/gasp/scans/#/*.info file"
		);
		goto fail;
	}
	
	if ( (ret =
		dump_files__open_file(
			&(dump->used), &space, inst, scan, ".used" )) != 0
	)
	{
		ERRMSG( ret,
			"Couldn't create/open %/gasp/scans/#/*.used file"
		);
		dump_files__shut_file( &(dump->info) );
		goto fail;
	}
	
	if ( (ret =
		dump_files__open_file(
			&(dump->data), &space, inst, scan, ".data" )) != 0
	)
	{
		ERRMSG( ret,
			"Couldn't create/open %/gasp/scans/#/*.data file"
		);
		dump_files__shut_file( &(dump->info) );
		dump_files__shut_file( &(dump->used) );
		goto fail;
	}
	
	fail:
	free_space( NULL, &space );
	return ret;
}

int dump_files_change_info( dump_t *dump ) {
	int ret = 0;
	ssize_t size;
	nodes_t *nodes;
	
	if ( !dump ) {
		ret = EDESTADDRREQ;
		ERRMSG( ret, "Must have somewhere to read data into" );
		return ret;
	}
		
	if ( gasp_lseek( dump->info.fd, 0, SEEK_SET ) != 0 ) {
		ret = errno;
		ERRMSG( ret, "Couldn't reset dump info file offset" );
		return ret;
	}
	
	size = sizeof(node_t);
	if ( gasp_write( dump->info.fd, &(dump->number), size ) != size ) {
		ret = errno;
		ERRMSG( ret, "Couldn't write scan number" );
		return ret;
	}
	
	size = sizeof(size_t);
	nodes = &(dump->nodes);
	if ( gasp_write( dump->info.fd, &(nodes->count), size ) != size ) {
		ret = errno;
		ERRMSG( ret, "Failed to write mappings count" );
		return ret;
	}
	
	return 0;
}

int dump_files_reset_offsets( dump_t *dump, bool read_info )
{	
	int ret = 0;
	nodes_t *nodes;
	space_t *space;
	(void)read_info;
	
	if ( !dump ) {
		ret = EDESTADDRREQ;
		ERRMSG( ret, "Must have a handle to work with" );
		return ret;
	}
	
	REPORT( "Clearing bytes done" )
	dump->done = 0;
	REPORT( "Clearing base index of buffer to start reading as data" )
	dump->base = 0;
	REPORT( "Clearing address that base would map to in source data" )
	dump->addr = 0;
	REPORT( "Clearing bytes kept from end of previous buffer" )
	dump->kept = 0;
	REPORT( "Clearing size of data read into buffer base refers to" )
	dump->size = 0;
	REPORT( "Clearing scan number" )
	dump->number = 0;
	
	REPORT( "Setting nodes pointer (for source memory info)" )
	nodes = &(dump->nodes);
	REPORT( "Setting space pointer (for smaller C code)" )
	space = &(nodes->space);
	
	REPORTF( "Clearing used address buffer, %p", dump->used.data )
	(void)memset( dump->used.data, 0, DUMP_LOC_NODE_SIZE );
	REPORT( "Clearing data buffer" )
	(void)memset( dump->data.data, 0, DUMP_LOC_NODE_SIZE );
	REPORT( "Clearing source memory info buffer" )
	(void)memset( space->block, 0, space->given );
	
	REPORT("Seeking to start of used addresses file")
	if ( gasp_lseek( dump->used.fd, 0, SEEK_SET ) != 0 ) {
		ret = errno;
		ERRMSG( ret, "Couldn't reset used addresses offset" );
		return ret;
	}
	
	REPORT("Seeking to start of data file")
	if ( gasp_lseek( dump->data.fd, 0, SEEK_SET ) != 0 ) {
		ret = errno;
		ERRMSG( ret, "Couldn't reset data offset" );
		return ret;
	}
	
	REPORT("Done with reset")
	return 0;
}

void dump_files_shut( dump_t *dump ) {
	if ( !dump )
		return;
	dump_files__shut_file( &(dump->info) );
	dump_files__shut_file( &(dump->used) );
	dump_files__shut_file( &(dump->data) );
	(void)dump_files_init( dump );
}

int dump_files_test( dump_t dump ) {
	if ( dump.info.fd <= 0 || dump.used.fd <= 0 || dump.data.fd <= 0 )
		return EBADF;
	return 0;
}

typedef uintmax_t (*func_proc_handle_rdwr_t)(
	int *err, proc_handle_t *handle,
		uintmax_t addr, void *buff, uintmax_t size
);

bool gasp_tpolli( int int_pipes[2], int *sig, int msTimeout )
{
	struct pollfd pfd = {0};
	if ( int_pipes[0] <= 0 )
		return 0;
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
	if ( ext_pipes[1] <= 0 )
		return 0;
	pfd.fd = ext_pipes[1];
	pfd.events = POLLOUT;
	if ( poll( &pfd, 1, msTimeout ) != 1 )
		return 0;
	write( ext_pipes[1], &sig, sizeof(int) );
	return 1;
}

bool gasp_thread_should_die( int ext_pipes[2], int int_pipes[2] )
{
	int ret = SIGCONT;
	if ( !gasp_tpolli( int_pipes, &ret, 0 ) )
		return 0;
	if ( ret == SIGKILL )
		return 1;
	while ( ret == SIGSTOP ) {
		if ( !gasp_tpollo( ext_pipes, SIGCONT, -1 ) )
			return 1;
		if ( !gasp_tpolli( int_pipes, &ret, -1 ) )
			return 1;
	}
	return (ret == SIGKILL);
}

node_t proc_handle_dump( tscan_t *tscan ) {
	int ret = 0;
	nodes_t nodes = {0};
	proc_mapped_t *src, *SRC, *dst, *DST;
	proc_handle_t *handle;
	uchar *used, *data;
	dump_t *dump;
	uintmax_t i, bytes;
	ssize_t size;
	mode_t prot = 04;
	
	if ( !tscan ) {
		ret = EDESTADDRREQ;
		ERRMSG( ret, "Was not given a scan thread object to work with" );
		return ret;
	}
	
	REPORT( "Getting current dump set pointer" )
	dump = &(tscan->dump[1]);
	REPORT( "Getting current dump used addresses pointer" )
	used = dump->used.data;
	REPORT( "Getting current dump data pointer" )
	data = dump->data.data;
	REPORT( "Clearing last amount found" )
	tscan->last_found = 0;
	REPORT( "Clearing addresss done upto" )
	tscan->done_upto = 0;
	REPORT( "Setting protection flags to check for" )
	prot |= tscan->writeable ? 02 : 0;
	REPORT( "Getting hooked process pointer" )
	handle = tscan->handle;
	
	REPORT( "Checking hooked process pointer" )
	if ( !handle ) {
		ret = EINVAL;
		ERRMSG( ret, "Invalid Process Handle" );
		return ret;
	}
	
	REPORT( "Checking current dump set file descriptor's" )
	if ( (ret = dump_files_test( *dump )) != 0 ) {
		ERRMSG( ret, "Focul dump files check failed" );
		goto done;
	}
	
	REPORT( "Min/Maxing what to zero data with" )
	if ( tscan->zero != 0 )
		tscan->zero = ~0;
	
	REPORT( "Reseting offesets" )
	/* No further need to change anything */
	if ( (ret = dump_files_reset_offsets( dump, 0 )) != 0 ) {
		ERRMSG( ret, "Focul dump files offsets reset failed" );
		goto done;
	}
	
	REPORT( "Reseting info" )
	if ( (ret = dump_files_change_info( dump )) != 0 ) {
		ERRMSG( ret, "Focul dump files info reset failed" );
		goto done;
	}
	
	/* Since this is a dump set all address booleans to true,
	 * a later scan can simply override the values,
	 * we use ~0 now because it's bit scan friendly,
	 * not that we're doing those yet */
	 REPORT( "Clearing dump data buffer" )
	(void)memset( dump->used.data, ~0, DUMP_LOC_NODE_SIZE );
	
	if ( (ret = proc_mapped_list( handle, &nodes )) != 0 )
	{
		ERRMSG( ret, "Failed to list mappings" );
		return ret;
	}
	
	if ( !(DST = more_nodes(
			proc_mapped_t, &ret, &(dump->nodes), nodes.count ))
	)
	{
		ERRMSG( ret, "Failed to allocate memory for mappings" );
		return ret;
	}
	
	SRC = nodes.space.block;
	
	REPORTF( "Started dump at %p", (void*)(dump->addr) )

	dump->nodes.focus = ~0;
	dump->nodes.count = 0;

	for ( nodes.focus = 0; nodes.focus < nodes.count; nodes.focus++ )
	{
		src = SRC + nodes.focus;
		tscan->done_upto = src->head;
		
		/* Not interested in out of range memory */
		if ( src->foot <= tscan->from || src->head >= tscan->upto )
			continue;
		
		for (
			bytes = 0;
			bytes < src->size;
			bytes += size
		)
		{
			i = src->size - bytes;
			if ( i > DUMP_LOC_NODE_SIZE ) i = DUMP_LOC_NODE_SIZE;
			
			if ( gasp_thread_should_die(
				tscan->main_pipes, tscan->scan_pipes )
			)
			{
				ret = EXIT_FAILURE;
				return ret;
			}
			
			tscan->done_upto = src->head + bytes;
			
			/* Prevent unread memory from corrupting locations found */
			(void)memset( data, tscan->zero, i );
			
			if ( (size = proc_glance_data(
				NULL, handle, src->head, data, i )) <= 0 )
				break;
			
			if ( gasp_write( dump->data.fd, data, size ) != size )
			{
				ret = errno;
				ERRMSG( ret, "Failed to write glanced at data" );
				goto done;
			}
		
			if ( gasp_write( dump->used.fd, used, size ) != size )
			{
				ret = errno;
				ERRMSG( ret, "Failed to write used addresses" );
				goto done;
			}
			
			fsync( dump->used.fd );
			fsync( dump->data.fd );
		}
		
		if ( bytes ) {
			
			/* Make sure no more than applicable is read in future */
			src->size = bytes;
			
			/* Get focused node, total is at least the same as source
			 * so no need to check if can add since the main loop check
			 * does that for us */
			dst = DST + (dump->nodes.count);
			
			/* Add the mapping to the list */
			memcpy( dst, src, sizeof(proc_mapped_t) );
			dump->nodes.count++;
		}
	}
	
#if VERBOSE
	fprintf( stderr, "Ended dump at %p\n", (void*)(dump->addr) );
#endif
	
	done:
	
	dump->number = tscan->done_scans;
	tscan->ret = dump_files_change_info(dump);
	
	fsync( dump->info.fd );
	tscan->ret = ret;
	
	/* Make sure we don't leak memory */
	free_nodes( proc_mapped_t, NULL, &nodes );
	
	return ret;
}

enum {
	PROC_MAPPED_PTR_VEC = 0,
	PROC_MAPPED_PTR_PRV,
	PROC_MAPPED_PTR_NOW,
	PROC_MAPPED_PTR_COUNT
};

int dump_files__mappings( dump_t *dump, proc_mapped_t **ptrs ) {
	nodes_t *nodes;
	proc_mapped_t *vec, *now;
	
	if ( !ptrs )
		return EINVAL;
	
	(void)memset( ptrs, 0, PROC_MAPPED_PTR_COUNT * sizeof(void*) );
	
	if ( !dump )
		return EINVAL;
		
	nodes = &(dump->nodes);
	
	vec = (proc_mapped_t*)(nodes->space.block);
	
	if ( !vec )
		return ENODATA;
	
	ptrs[PROC_MAPPED_PTR_VEC] = vec;
	
	if ( nodes->focus >= nodes->total )
		/* Easier to check against for multi-source handlers */
		return EOF;
	
	now = vec + nodes->focus;
	
	ptrs[PROC_MAPPED_PTR_PRV] = (now != vec) ? now - 1 : NULL;
	ptrs[PROC_MAPPED_PTR_NOW] = now;
	
	return 0;
}

int dump_files_glance_stored( dump_t *dump, size_t keep )
{
	int ret = 0;
	ssize_t expect = 0, size = 0;
	uchar *data, *used;
	proc_mapped_t *pmap, *nmap, *ptrs[PROC_MAPPED_PTR_COUNT] = {NULL};
	nodes_t *nodes;
	
	if ( (ret = dump_files__mappings( dump, (proc_mapped_t**)(&ptrs) )) != 0 )
	{
		if ( ret == EINVAL )
			return EDESTADDRREQ;
		return ret;
	}
	
	/* Initialise pointers */
	used = dump->used.data;
	data = dump->data.data;
	
	nodes = &(dump->nodes);
	nmap = ptrs[PROC_MAPPED_PTR_NOW];
	pmap = ptrs[PROC_MAPPED_PTR_PRV];
	
	dump->kept = 0;
	
	/* Update pointers to change region */
	if ( dump->done == nmap->size )
	{
		dump->done = 0;
		dump->addr = nmap->foot;
		dump->nodes.focus++;
		
		if ( nodes->focus == nodes->total )
			return EOF;

		pmap = ++nmap - 1;
		dump->addr = nmap->head;
	}
	
	expect = nmap->size - dump->done;
	if ( expect > DUMP_LOC_NODE_HALF ) expect = DUMP_LOC_NODE_HALF;
	dump->base = DUMP_LOC_NODE_HALF;
	dump->last = DUMP_LOC_NODE_HALF;
	
	(void)memmove( used,
		used + DUMP_LOC_NODE_HALF, DUMP_LOC_NODE_HALF );
	(void)memmove( data,
		data + DUMP_LOC_NODE_HALF, DUMP_LOC_NODE_HALF );
	
	if ( (ret = dump_files__read_file(
		&(dump->used), used + DUMP_LOC_NODE_HALF, expect, &size))
		!= 0 || size != expect
	)
	{
		fprintf( stderr,
			"(used.fd) 0x%jX - 0x%jX (0x%jX bytes, 0x%jX read), "
			"fd = %d, ptr = %p, size = %zd\n, expect = %zd",
			nmap->head, nmap->foot, nmap->size, dump->done,
			dump->used.fd, used + DUMP_LOC_NODE_HALF, size, expect );
		goto failure;
	}
	
	if ( (ret = dump_files__read_file(
		&(dump->data), data + DUMP_LOC_NODE_HALF, expect, &size))
		!= 0 || size != expect
	)
	{
		fprintf( stderr,
			"(data.fd) 0x%jX - 0x%jX (0x%jX bytes, 0x%jX read), "
			"fd = %d, ptr = %p, size = %zd\n, expect = %zd",
			nmap->head, nmap->foot, nmap->size, dump->done,
			dump->data.fd, data + DUMP_LOC_NODE_HALF, size, expect );
		goto failure;
	}
	
	dump->tail = DUMP_LOC_NODE_SIZE - size;
	dump->last = dump->tail - keep;
	
	if ( dump->done )
		dump->addr = nmap->head + dump->done;
	dump->size = size;
	
	if ( dump->done || dump->addr == pmap->foot )
	{
		if ( keep >= DUMP_LOC_NODE_HALF ) {
			ret = ERANGE;
			ERRMSG( ret, "Number of bytes to keep was too large" );
			goto failure;
		}
		
		dump->addr -= keep;
		dump->kept = keep;
		dump->base -= DUMP_LOC_NODE_HALF;
	}
	dump->done += size;
	
	return 0;
	
	failure:
	if ( ret == ENOMEM )
		fprintf( stderr, "Requested 0x%08jX bytes\n", size );
	return ret;
}

bool dump_files_change_stored( dump_t *dump )
{
	ssize_t pos;
	
	if ( !dump )
		return 0;

	pos = dump->tail - dump->base;
	gasp_lseek( dump->used.fd, -pos, SEEK_SET );
	gasp_lseek( dump->data.fd, -pos, SEEK_SET );
	gasp_write( dump->used.fd, dump->used.data + dump->base, pos );
	gasp_write( dump->data.fd, dump->data.data + dump->base, pos );
	
	fsync( dump->used.fd );
	fsync( dump->data.fd );
	return 1;
}

int proc_handle__aobscan_next( tscan_t *tscan, dump_t *dump )
{
	if ( !tscan )
		return EDESTADDRREQ;
	if ( gasp_thread_should_die(
		tscan->main_pipes, tscan->scan_pipes ) )
		return ECANCELED;
	return dump_files_glance_stored( dump, tscan->bytes - 1 );
}

int proc_handle__aobscan_test( tscan_t *tscan )
{
	uchar *used, *data;
	uintmax_t addr, a, *ptrs;
	nodes_t *nodes;
	dump_t *dump;
	
	if ( !tscan )
		return EDESTADDRREQ;
	
	dump = &(tscan->dump[1]);
	nodes = &(tscan->locations);
	ptrs = nodes->space.block;
	used = dump->used.data;
	data = dump->data.data;
	addr = dump->addr;
	
	for ( a = dump->base; a < dump->last; ++a, ++addr )
	{
		if ( used[a] ) {
			used[a] = (
				memcmp( data + a, tscan->array, tscan->bytes ) == 0
			);
			
			fprintf( stderr, "Checked 0x%jX for value %u, %d vs %d\n",
				addr, used[a],
				*((short*)(data + a)),
				*((short*)(tscan->array)) );
			
			if ( used[a] ) {
				tscan->found++;
				if ( nodes->count < nodes->total )
					ptrs[(nodes->count)++] = addr;
			}
		}
		tscan->done_upto = addr;
	}
	
	return dump_files_change_stored( dump );
}

int proc_handle_aobscan( tscan_t *tscan ) {
	int ret = 0;
	uintmax_t minus;
	mode_t prot = 04;
	bool have_prev_results = 0;
	dump_t *dump, *prev;
	
	if ( !tscan ) {
		ret = EDESTADDRREQ;
		ERRMSG( ret, "bytescanner was given NULL" );
		return ret;
	}
	
	minus = tscan->bytes - 1;
	tscan->found = 0;
	tscan->done_upto = 0;
	prev = tscan->dump;
	dump = prev + 1;
	
	if ( minus > tscan->bytes || tscan->bytes >= DUMP_LOC_NODE_HALF ) {
		ret = EINVAL;
		ERRMSG( ret, "Too many bytes!" );
		goto set_found;
	}

	if ( (ret = dump_files_test( *dump )) != 0 ) {
		ERRMSG( ret, "Invalid dump to scan" );
		goto set_found;
	}

	prot |= tscan->writeable ? 02 : 0;
	
	if ( (ret =
		proc__rwvmem_test(tscan->handle, tscan->array, tscan->bytes ))
		!= 0
	)
	{
		fprintf( stderr, "Failed at proc__rwvmen_test()\n" );
		goto set_found;
	}
	
	if ( (ret = dump_files_reset_offsets( dump, 1 )) != 0 ) {
		fprintf( stderr, "Failed at dump_files_reset_offsets()\n" );
		goto set_found;
	}
	
	if ( dump_files_test( *prev ) == 0 ) {
		
		if ( (ret = dump_files_reset_offsets( prev, 1 ))
			!= 0
		)
		{
			fprintf( stderr, "Failed at 2nd dump_files_reset_offsets()\n" );
			goto set_found;
		}
		
		have_prev_results = 1;
	}
	
	/* Make sure we start at the first recorded mapping */
	dump->nodes.focus = 0;

#if VERBOSE
	fprintf( stderr, "Started scan at %p\n", (void*)(dump->addr) );
#endif

	while ( (ret = proc_handle__aobscan_next( tscan, dump )) == 0 )
	{
#if VERBOSE
		fprintf( stderr, "%p-%p\n",
			(void*)(dump->addr), (void*)(dump->addr + dump->size) );
#endif
		
		if ( have_prev_results ) {
			/* Ensure we check only addresses that have been marked
			 * as a result previously */
			if (
				(ret = dump_files_glance_stored(
					prev, tscan->bytes - 1 )) != 0
			)
			{
				ERRMSG( ret, "Couldn't glance at previous results" );
				goto set_found;
			}
			(void)memcpy(
				dump->used.data, prev->used.data, DUMP_LOC_NODE_SIZE );
		}
		else {
			/* Ensure we check all addresses */
			(void)memset( dump->used.data, ~0, DUMP_LOC_NODE_SIZE );
		}
		
		if ( !proc_handle__aobscan_test( tscan ) )
			goto set_found;
	}
	
#if VERBOSE
	fprintf( stderr, "Ended scan at %p\n", (void*)(dump->addr) );
#endif
	
	tscan->done_upto = tscan->upto;
	
	set_found:
	tscan->last_found = tscan->found;
	tscan->done_scans++;
	
	switch ( ret ) {
	case 0: case EOF: case ECANCELED:
		ret = 0;
		break;
	default:
		ERRMSG( ret, "Byte scan failed to run fully" );
	}
	
	return ret;
}

void* proc_handle_dumper( void * _tscan ) {
	int ret = 0;
	tscan_t *tscan = _tscan;
	
	if ( !tscan )
		return NULL;
		
	tscan->threadmade = 1;
	tscan->found = 0;
	
#if VERBOSE
	fprintf( stderr, "Starting dump" );
#endif

	tscan->dumping = 1;
	ret = proc_handle_dump( tscan );
	tscan->dumping = 0;
	
	if ( ret != 0 ) {
		ERRMSG( ret, "Unable to complete dump, aborting..." );
		goto fail;
	}

#if VERBOSE
	fprintf( stderr, "Starting scan" );
#endif
	
	tscan->scanning = 1;
	ret = proc_handle_aobscan( tscan );
	tscan->scanning = 0;
	
	if ( ret != 0 ) {
		ERRMSG( ret, "Unable to complete scan, aborting..." );
		goto fail;
	}

#if VERBOSE
	fprintf( stderr, "Finished" );
#endif
	
	fail:
	gasp_tpollo( tscan->main_pipes, SIGCONT, -1 );
	tscan->threadmade = 0;
	tscan->ret = ret;
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
	int ret = 0;
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
	(void)memset( nodes, 0, sizeof(nodes_t) );
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
			!= 0 )
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
	if ( ret == 0 ) ret = ENOENT;
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
	
	(void)memset( name->block, 0, name->given );
	(void)memset( cmdl->block, 0, cmdl->given );
	(void)memset( full->block, 0, full->given );
	
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
	

	if ( err && *err != 0 ) {
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
	int ret = errno = 0;
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
					!= 0 )
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
	ret = ( errno != 0 ) ? errno : EXIT_FAILURE;
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
	
	if ( ret != 0 ) {
		if ( err ) *err = ret;
		return 0;
	}
	
	errno = 0;
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

	if ( ret != 0 ) {
		if ( err ) *err = ret;
		return 0;
	}
	
	errno = 0;
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
