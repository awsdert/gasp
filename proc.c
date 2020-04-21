#include "gasp.h"
int process_stop( process_t *process ) {
	int ret = 0;
	(void)kill( process->pid, SIGSTOP );
	(void)waitpid( process->pid, &ret, WUNTRACED );
	process->paused = 1;
	return 0;
}
int process_cont( process_t *process ) {
	if ( process->paused ) {
		if ( kill( process->pid, SIGCONT ) != 0 )
			return errno;
		process->paused = 0;
	}
	return 0;
}
int process_grab( process_t *process ) {
	int ret = 0;
	if ( !(process->hooked) ) {
		errno = 0;
		if ( ptrace( PTRACE_ATTACH, process->pid, NULL, NULL ) != 0 )
			return errno;
		(void)waitpid( process->pid, &ret, WSTOPPED );
		process->hooked = 1;
	}
	return ret;
}
int process_free( process_t *process ) {
	if ( process->hooked ) {
		if ( ptrace( PTRACE_DETACH, process->pid, NULL, NULL ) != 0 )
			return errno;
		process->hooked = 0;
	}
	return 0;
}
int file_glance_perm( char const *path, mode_t *perm ) {
	struct stat st;
	
	if ( !perm )
		return EDESTADDRREQ;
		
	*perm = 0;
	if ( !path )
		return EINVAL;
	
	errno = 0;
	if ( stat( path, &st ) != 0 )
		return errno;
	
	*perm = st.st_mode;
	return 0;
}

int file_change_perm( char const * path, mode_t perm ) {
	
	if ( !path )
		return EINVAL;
	
	errno = 0;
	if ( chmod( path, perm ) != 0 )
		return errno;
	
	return 0;
}

int file_glance_size( char const *path, ssize_t *size )
{
	int fd = -1, ret = 0;
	char buff[BUFSIZ], *pof;
	gasp_off_t bytes = 0, done = -1;
	struct stat st;
	
	pof = "size";
	if ( !size )
	{
		ret = EDESTADDRREQ;
		goto fail;
	}
	
	*size = 0;
	pof = "path";
	if ( !path )
	{
		ret = EINVAL;
		goto fail;
	}
	
	pof = "stat";
	errno = 0;
	if ( stat( path, &st ) != 0 )
	{
		ret = errno;
		goto fail;
	}
	
	if ( st.st_size > 0 )
	{
		*size = st.st_size;
		return 0;
	}
	
	pof = "open";
	errno = 0;
	if ( (fd = open(path,O_RDONLY)) < 0 )
	{
		ret = errno;
		switch ( ret ) {
		case ENOENT: case ENOTDIR:
			return 0;
		}
		goto fail;
	}
	
	pof = "read";
	while ( (done = gasp_read( fd, buff, BUFSIZ )) < 0 )
		bytes += done;

	ret = errno;
	
	if ( ret != 0 && ret != EOF )
	{
		fail:
		FAILED( ret, pof );
		return ret;
	}
	
	if ( fd > 0 )
		close(fd);
	
	*size = bytes;
	return 0;
}

void* process__wait4exit( void *data ) {
	process_t *process = data;
	process->thread.active = 1;
	while ( process->thread.active ) {
		if ( access( process->path, F_OK ) != 0 ) {
			process->exited = 1;
			process->thread.active = 0;
		}
	}
	return data;
}

#define PAGE_LINE_SIZE ((sizeof(void*) * 4) + 7)

int mapped_list( process_t *process, nodes_t *nodes )
{
	int ret = 0, fd = -1;
	mapped_t m = {0}, *mv = NULL;
	ssize_t size = 0;
	char *line, *pos, path[SCHAR_MAX] = {0}, *pof = NULL;
	space_t space = {0};
	node_t n = 0;
	
	pof = "nodes check";
	if ( !nodes )
	{
		ret = EDESTADDRREQ;
		goto fail;
	}
	
	REPORT("Clearing data stored in nodes parameter")
	nodes->focus = nodes->count = 0;
	nodes->total = nodes->space.given / sizeof(mapped_t);
	if ( nodes->space.block )
	{
		pof = "nodes data cleanup";
		errno = 0;
		if ( !memset( nodes->space.block, 0, nodes->space.given ) )
		{
			ret = errno;
			goto fail;
		}
	}
	
	pof = "process check";
	if ( !process )
	{
		ret = EINVAL;
		goto fail;
	}
	
	(void)sprintf( path, "%s/maps", process->path );
	pof = "glance size";
	if ( (ret = file_glance_size( path, &size )) != 0 )
		goto fail;
	
	pof = "more space";
	if ( (ret = more_space( &space, size + 1 )) != 0 )
		goto fail;
	
	pof = "more nodes";
	if ( (ret = more_nodes( mapped_t, nodes, 50 )) != 0 )
		goto fail;
	
	pof = "open";
	if ( (fd = open( path, O_RDONLY )) < 0 )
	{
		ret = errno;
		goto fail;
	}
	
	pof = "read";
	if ( (size = gasp_read( fd, space.block, space.given )) <= 0 )
	{
		ret = errno;
		goto fail;
	}
	
	close(fd);
	fd = -1;
	
	for ( line = space.block; *line; ++line )
	{
		pof = "object cleanup";
		errno = 0;
		if ( !memset( &m, 0, sizeof(mapped_t) ) )
		{
			ret = errno;
			goto fail;
		}
		
		/* Read address range */
		pof = "address range";
		errno = 0;
		if ( sscanf( line, "%jx-%jx", &(m.head), &(m.foot) ) < 0 )
		{
			ret = errno;
			goto fail;
		}
		
		/* Get position right after address range */
		pos = strchr( line, ' ' );
		if ( !pos )
			break;
		
		/* Clear so that only the range is used for file name */
		*pos = 0;
		
		/* Move pointer to the permissions text */
		++pos;
		
		m.size = (m.foot) - (m.head);
		m.prot |= (pos[0] == 'r') ? 04 : 0;
		m.prot |= (pos[1] == 'w') ? 02 : 0;
		m.prot |= (pos[2] == 'x') ? 01 : 0;
		/* Check if shared */
		m.prot |= (pos[3] == 'p') ? 0 : 010;
		if ( m.prot & 010 )
			/* Check if validated share */
			m.prot |= (pos[3] == 's') ? 0 : 020;
		
		pof = "map files";
		errno = 0;
		if ( sprintf( path,
			"%s/map_files/%s", process->path, line ) < 0
		)
		{
			ret = errno;
			goto fail;
		}
		
		pof = "glance perm";
		if ( (ret = file_glance_perm( path, &(m.perm) )) != 0 )
			goto fail;
			
		pof = "add node";
		if ( (ret = add_node( nodes, &n, sizeof(mapped_t))) != 0 )
			goto fail;
		mv = nodes->space.block;
		mv[n] = m;
		
		for ( line = pos; *line != '\n'; ++line )
		{
			if ( *line == '\0' )
				break;
		}
	}
	pof = NULL;
	
	if ( pof )
	{
		fail:
		FAILED( ret, pof );
		EOUTF( "path = '%s'", path );
	}
	if ( fd >= 0 ) close(fd);
	free_space( &space );
	return ret;
}

int mapped_next(
	process_t *process, mode_t prot, mapped_t *mapped
)
{
	int ret = 0;
	nodes_t nodes = {0};
	node_t n;
	mapped_t *m, *mv;
	char * pof;
	
	pof = "mapped pointer check";
	if ( !mapped ) {
		ret = EDESTADDRREQ;
		goto fail;
	}
	
	pof = "list mappings";
	if ( (ret = mapped_list( process, &nodes )) != 0 )
		goto fail;
	mv = nodes.space.block;
	
	for ( n = 0; n < nodes.count; ++n ) {
		m = mv + n;
		if ( mapped->foot <= m->foot || (m->prot & prot) != prot )
			continue;
		
		(void)memcpy( mapped, m, sizeof(mapped_t) );
		break;
	}
	
	if ( n == nodes.count )
	{
		(void)memset( mapped, 0, sizeof(mapped_t) );
		ret = ERANGE;
	}
	
	if ( ret != 0 && ret != ERANGE )
	{
		fail:
		FAILED( ret, pof );
	}
	free_nodes( &nodes );
	return ret;
}

int mapped_addr(
	process_t *process, mapped_t *mapped, uintmax_t addr
)
{
	int ret = 0;
	nodes_t nodes = {0};
	node_t n;
	mapped_t *m, *mv;
	char *pof;
	
	pof = "mapped pointer check";
	if ( !mapped )
	{
		ret = EDESTADDRREQ;
		goto fail;
	}
	
	pof = "list mappings";
	if ( (ret = mapped_list( process, &nodes )) != 0 )
		return ret;
	mv = nodes.space.block;
	
	for ( n = 0; n < nodes.count; ++n ) {
		m = mv + n;
		if ( addr < m->head || addr > m->foot )
			continue;
		
		(void)memcpy( mapped, m, sizeof(mapped_t) );
		break;
	}
	
	if ( n == nodes.count )
	{
		(void)memset( mapped, 0, sizeof(mapped_t) );
		ret = ERANGE;
	}
	
	if ( ret != 0 && ret != ERANGE )
	{
		fail:
		FAILED( ret, pof );
	}
	free_nodes( &nodes );
	return ret;
}

int proc__rwvmem_test(
	process_t *process, void *mem, ssize_t size, ssize_t *done
)
{
	int ret = 0;
	char *pof;
	
	pof = "bytes done pointer check";
	if ( !done )
	{
		ret = EDESTADDRREQ;
		goto fail;
	}
	
	*done = 0;
	if ( !process || !mem || size <= 0 || !done )
	{
		ret = EINVAL;
		if ( !process ) pof= "process handle check";
		if ( !mem ) pof = "memory pointer check";
		if ( size <= 0 ) pof = "memory size check";
		goto fail;
	}
	
	if ( ret != 0 )
	{
		fail:
		FAILED( ret, pof );
	}
	
	return ret;
}

int proc__glance_peek(
	process_t *process,
	uintmax_t addr, void *mem, ssize_t size, ssize_t *done
)
{
	uintmax_t dst = 0, predone = 0;
	int ret;
	uchar *m = mem;
	size_t temp;
	
	if ( (ret = proc__rwvmem_test(process, mem,size,done)) != 0 )
		return ret;

	errno = 0;
	
	for ( temp = 0; addr % sizeof(long); ++temp, --addr );
	if ( temp ) {
		dst = ptrace( PTRACE_PEEKDATA,
			process->pid, (void*)addr, NULL );
		if ( errno != 0 )
			return 0;
		temp = (sizeof(long) - (predone = temp));
		memcpy( m, (&dst) + predone, temp );
		addr += sizeof(long);
		size -= predone;
	}
	
	while ( *done < size ) {
		dst = ptrace( PTRACE_PEEKTEXT,
			process->pid, (void*)(addr + done), NULL );
		
		if ( (ret = errno) != 0 )
		{
			done += predone;
			return ret;
		}
		
		temp = size - *done;
		if ( temp > sizeof(long) ) temp = sizeof(long);
		memcpy( m + predone + *done, &dst, temp );
		*done += temp;
	}
	*done += predone;
	
	return 0;
}

int proc__glance_vmem(
	process_t *process,
	uintmax_t addr, void *mem, ssize_t size, ssize_t *done
)
{
#ifdef _GNU_SOURCE
	struct iovec internal, external;
	int ret;
	uchar *m = mem;
	char *pof;
	
	pof = "parameter check";
	if ( (ret = proc__rwvmem_test(process,mem,size,done)) != 0 )
		goto fail;
	
	internal.iov_base = m;
	internal.iov_len = size;
	external.iov_base = (void*)addr;
	external.iov_len = size;
	
	pof = "read process memory";
	errno = 0;
	if ( (*done = process_vm_readv(
		process->pid, &internal, 1, &external, 1, 0)) <= 0 )
	{
		ret = errno;
		goto fail;
	}
	
#else
	ret = ENOSYS;
#endif

	if ( ret != 0 )
	{
#ifdef _GNU_SOURCE
		fail:
#endif
		FAILED( ret, pof );
		EOUTF( "external.iov_base = %p", external.iov_base );
	}
	
	return ret;
}

int proc__glance_file(
	process_t *process,
	uintmax_t addr, void *mem, ssize_t size, ssize_t *done
)
{
#ifdef gasp_pread
	int ret = 0, fd = -1;
	char path[bitsof(int) * 2] = {0}, *pof;
	
	pof = "parameter check";
	if ( (ret = proc__rwvmem_test(process,mem,size,done)) != 0 )
		goto fail;
	
	sprintf( path, "%s/mem", process->path );
	pof = "open file";
	errno = 0;
	if ( (fd = open(path,O_RDONLY)) <= 0 )
	{
		ret = errno;
		goto fail;
	}
	
	pof = "read file";
	if ( (*done = gasp_pread( fd, mem, size, addr )) <= 0 )
	{
		ret = errno;
		if ( *done < 0 )
			goto fail;
	}

	switch ( ret )
	{
	case 0: case ESPIPE: case EIO: break;
	default:
		fail:
		FAILED( ret, pof );
	}
	
	if ( fd > 0 )
		close( fd );
	return ret;
#else
	return ENOSYS;
#endif
}

int proc__glance_self(
	process_t *process,
	uintmax_t addr, void *mem, size_t size, ssize_t *done
)
{
	int ret;
	char *pof;
	
	pof = "parameter check";
	if ( (ret = proc__rwvmem_test(process,mem,size,done)) != 0 )
		goto fail;
	
	if ( process->self ) {
		pof = "memmove";
		errno = 0;
		(void)memmove( mem, (void*)addr, size );
		ret = errno;
		if ( ret != 0 )
			goto fail;
		
		*done = size;
	}
	
	if ( ret != 0 )
	{
		fail:
		FAILED( ret, pof );
	}
	
	return ret;
}

int proc__glance_seek(
	process_t *process,
	uintmax_t addr, void *mem, size_t size, ssize_t *done
)
{
	char path[bitsof(int)*2] = {0};
	int ret, fd;
	
	if ( (ret = proc__rwvmem_test(process,mem,size,done)) != 0 )
		return ret;
	
	sprintf( path, "%s/mem", process->path );
	if ( (fd = open( path, O_RDONLY )) < 0 )
		return errno;
	
	errno = 0;
	gasp_lseek( fd, addr, SEEK_SET );
	if ( (ret = errno) != 0 )
		goto failed;
	
	errno = 0;
	if ( (*done = gasp_read( fd, mem, size )) <= 0 )
		ret = errno;
	
	failed:
	close(fd);
	return ret;
}

int proc__change_poke(
	process_t *process,
	uintmax_t addr, void *mem, ssize_t size, ssize_t *done
)
{
	uintmax_t dst = 0;
	int ret;
	uchar *m = mem;
	size_t temp;
	
	if ( (ret = proc__rwvmem_test(process,mem,size,done)) != 0 )
		return ret;

	if ( (process->flags & 02) != 02 )
		return EPERM;
	
	while ( *done < size ) {
		errno = 0;
		dst = ptrace(
			PTRACE_PEEKDATA, process->pid, addr + *done, NULL );
		if ( (ret = errno) != 0 )
			break;
			
		temp = size - *done;
		if ( temp > sizeof(long) ) temp = sizeof(long);
		memcpy( &dst, m + *done, temp );
		errno = 0;
		ptrace(
			PTRACE_POKEDATA, process->pid, addr + *done, &dst );
		ret = errno;
		done += temp;
	}
	
	return ret;
}

int proc__change_vmem(
	process_t *process,
	uintmax_t addr, void *mem, ssize_t size, ssize_t *done
)
{
#ifdef _GNU_SOURCE
	struct iovec internal, external;
	int ret;
	
	if ( (ret = proc__rwvmem_test(process, mem,size,done)) != 0 )
		return ret;
	
	if ( (process->flags & 02) != 02 )
		return EPERM;
	
	internal.iov_base = mem;
	internal.iov_len = size;
	external.iov_base = (void*)addr;
	external.iov_len = size;
	
	errno = 0;
	if ( (*done = process_vm_writev(
		process->pid, &internal, 1, &external, 1, 0)) != size )
		return errno;
	
	return 0;
#else
	return = ENOSYS;
#endif
}

int proc__change_file(
	process_t *process,
	uintmax_t addr, void *mem, ssize_t size, ssize_t *done
)
{
#ifdef gasp_pwrite
	int ret = 0, fd = -1;
	char path[bitsof(int)*2] = {0};
	
	if ( (ret = proc__rwvmem_test(process,mem,size,done)) != 0 )
		return ret;
	
	if ( (process->flags & 02) != 02 )
		return EPERM;
		
	(void)sprintf( path, "%s/mem", process->path );
	
	errno = 0;
	if ( (fd = open(path,O_WRONLY)) < 0 )
		return errno;
	
	errno = 0;
	if ( (*done = gasp_pwrite( fd, mem, size, addr )) <= 0 )
		ret = errno;

	close(fd);
	return ret;
#else
	return ENOSYS;
#endif
}

int proc__change_self(
	process_t *process,
	uintmax_t addr, void *mem, ssize_t size, ssize_t *done
)
{
	int ret;
	
	if ( (ret = proc__rwvmem_test(process,mem,size,done)) != 0 )
		return ret;
	
	if ( (process->flags & 02) != 02 )
		return EPERM;
	
	if ( process->self )
	{
		errno = 0;
		(void)memmove( (void*)addr, mem, size );
		if ( (ret = errno) != 0 )
			return ret;
		*done = size;
	}
	
	return 0;
}

int proc__change_seek(
	process_t *process,
	uintmax_t addr, void *mem, ssize_t size, ssize_t *done
)
{
	char path[bitsof(int) * 2] = {0};
	int ret, fd;
	
	if ( (ret = proc__rwvmem_test(process,mem,size,done)) != 0 )
		return ret;
	
	sprintf( path, "%s/mem", process->path );
	
	errno = 0;
	if ( (fd = open( path, O_WRONLY )) < 0 )
		return errno;
	
	errno = 0;
	if ( gasp_lseek( fd, addr, SEEK_SET ) < 0 )
	{
		ret	= errno;
		goto failed;
	}
	
	errno = 0;
	if ( (*done = gasp_write( fd, mem, size )) <= 0 )
		ret = errno;
		
	failed:
	close(fd);
	return ret;
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
	if ( (ret = more_space( space, strlen( path ) + 9 )) != 0 )
		return ret;
	temp = space->block;
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
	if ( (ret = more_space( space, len )) != 0 )
		return ret;
	path = space->block;

	sprintf( path, "%s/scans/%ld/%ld%s", GASP_PATH, inst, scan, ext );
	return gasp_open( &(dump_file->fd), path, 0700 );
}

int dump_files__read_file(
	dump_file_t *dump_file, void *dst, ssize_t size, ssize_t *done
)
{
	int ret = 0;
	char *pof;
	
	pof = "parameter check";
	if ( !dump_file || !dst || size < 1 || !done )
	{
		ret = EINVAL;
		goto fail;
	}

	pof = "read dump file offset";
	errno = 0;
	if ( (dump_file->prv =
		gasp_lseek( dump_file->fd, 0, SEEK_CUR )) < 0
	)
	{
		ret = errno;
		goto fail;
	}
	
	pof = "read dump file data";
	errno = 0;
	if ( (*done = gasp_read( dump_file->fd, dst, size )) < 0 )
	{
		ret = errno;
		goto fail;
	}
	
	pof = "store new dump file offset";
	errno = 0;
	if ( (dump_file->pos =
		gasp_lseek( dump_file->fd, 0, SEEK_CUR )) < 0
	)
	{
		ret = errno;
		goto fail;
	}
	
	if ( ret != 0 )
	{
		fail:
		if ( ret == 0 ) ret = EIO;
		FAILED( ret, pof );
	}
	
	return ret;
}

int dump_files__write_file(
	dump_file_t *dump_file, void *dst, ssize_t size, ssize_t *done
)
{
	int ret = 0;
	char *pof;
	
	pof = "parameter check";
	if ( !dump_file || !dst || size < 1 || !done )
		return EINVAL;

	pof = "store dump file offset";
	errno = 0;
	if ( (dump_file->prv =
		gasp_lseek( dump_file->fd, 0, SEEK_CUR )) < 0
	)
	{
		ret = errno;
		goto fail;
	}
	
	pof = "write dump file data";
	errno = 0;
	if ( (*done = gasp_write( dump_file->fd, dst, size )) < 0 )
	{
		ret = errno;
		goto fail;
	}
	
	pof = "store new dump file offset";
	errno = 0;
	if ( (dump_file->pos =
		gasp_lseek( dump_file->fd, 0, SEEK_CUR )) < 0
	)
	{
		ret = errno;
		goto fail;
	}
	
	if ( ret != 0 )
	{
		fail:
		if ( ret == 0 ) errno = EIO;
		FAILED( ret, pof );
	}
	
	return ret;
}

int dump_files__dir( space_t *space, long inst, bool make ) {
	int ret = 0;
	char const *GASP_PATH = NULL, *pof;
	char *path = NULL;
	size_t len;
	
	pof = "destination of path text check";
	if ( !space )
	{
		ret = EDESTADDRREQ;
		goto fail;
	}
	
	GASP_PATH = getenv("GASP_PATH");
	pof = "GASP_PATH check";
	if ( !GASP_PATH )
	{
		ret = EINVAL;
		goto fail;
	}
	
	len = strlen(GASP_PATH) + (2 * (sizeof(node_t) * CHAR_BIT));
	
	if ( (ret = more_space( space, len )) != 0 )
		goto fail;
	path = space->block;
	
	pof = GASP_PATH;
	if ( (ret = gasp_isdir( GASP_PATH, make )) != 0 )
		return ret;
	
	pof = path;
	sprintf( path, "%s/scans", GASP_PATH );
	if ( (ret = gasp_isdir( path, make )) != 0 )
		goto fail;
	
	sprintf( path, "%s/scans/%ld", GASP_PATH, inst );
	if ( (ret = gasp_isdir( path, make )) != 0 )
		goto fail;
	
	if ( ret != 0 )
	{
		fail:
		FAILED( ret, pof );
		EOUTF( "GASP_PATH = '%s'", GASP_PATH ? GASP_PATH : "(null)" );
		EOUTF( "path = '%s'", path ? path : "(null)" );
	}
	
	return ret;
}

int dump_files_init( dump_t *dump ) {
	uint i;
	uchar *_;
	nodes_t *nodes;
	space_t *space;
	
	if ( !dump )
		return EDESTADDRREQ;
	
	REPORT("Setting C code pointers")
	nodes = dump->nodes;
	space = &(nodes->space);
		
	REPORT("Clearing entire object")
	(void)memset( dump, 0, sizeof( dump_t ) );
	
	REPORT("Forcing invalid files descriptors")
	dump->info.fd = dump->used.fd = dump->data.fd = -1;
	
	REPORT("Restoring pointer to nodes_t object holding mapping info");
	dump->nodes = nodes;
	
	REPORT("Calculating local buffer pointers")
	_ = dump->_;
	for ( i = 0; i < DUMP_LOC_NODE_UPTO; ++i ) {
		dump->__[i] = _ + (i * DUMP_LOC_NODE_SIZE);
	}
	
	REPORT("Setting named pointers to calculated pointers")
	REPORTF("Setting named pointer for info, %p", (void*)space )
	dump->info.data = space->block;
	REPORT("Setting named pointer for used address")
	dump->used.data = dump->__[DUMP_LOC_NODE_USED];
	REPORT("Setting named pointer for data")
	dump->data.data = dump->__[DUMP_LOC_NODE_DATA];
	
	REPORT("Setting named sizes to correct sizes")
	dump->info.size = space->given;
	dump->used.size = DUMP_LOC_NODE_SIZE;
	dump->data.size = DUMP_LOC_NODE_SIZE;
	
	return 0;
}

int dump_files_open( dump_t *dump, long inst, long scan ) {
	int ret = 0;
	space_t space = {0};
	char *pof;
	
	pof = "dump object check";
	if ( !dump )
	{
		ret = EDESTADDRREQ;
		goto errmsg;
	}
	
	/* Cleanse structure of left over data data */
	if ( dump_files_test( *dump ) == 0 )
		dump_files_shut( dump );
	else
		(void)dump_files_init( dump );
	
	pof = "create any missing directories"; 
	if ( (ret = dump_files__dir( &space, inst, 1 )) != 0 )
		goto fail;

	pof = "open info file";
	if ( (ret =
		dump_files__open_file(
			&(dump->info), &space, inst, scan, ".info" )) != 0
	) goto fail;
	
	pof = "open used pointers file";
	if ( (ret =
		dump_files__open_file(
			&(dump->used), &space, inst, scan, ".used" )) != 0
	) goto fail;
	
	pof = "open data file";
	if ( (ret =
		dump_files__open_file(
			&(dump->data), &space, inst, scan, ".data" )) != 0
	) goto fail;
	
	if ( ret != 0 )
	{
		fail:
		dump_files__shut_file( &(dump->info) );
		dump_files__shut_file( &(dump->used) );
		dump_files__shut_file( &(dump->data) );
		errmsg:
		FAILED( ret, pof );
	}
	
	free_space( &space );
	return ret;
}

int dump_files_change_info( dump_t *dump ) {
	int ret = 0;
	char *pof;
	ssize_t size;
	nodes_t *nodes;
	
	pof = "dump object check";
	if ( !dump )
	{
		ret = EDESTADDRREQ;
		goto fail;
	}
		
	pof = "reset info offset";
	if ( gasp_lseek( dump->info.fd, 0, SEEK_SET ) != 0 )
	{
		ret = errno;
		goto fail;
	}
	
	size = sizeof(node_t);
	pof = "write scan number";
	errno = 0;
	if ( gasp_write( dump->info.fd, &(dump->number), size ) != size )
	{
		ret = errno;
		goto fail;
	}
	
	nodes = dump->nodes;
	pof = "write mappings count";
	errno = 0;
	if ( gasp_write( dump->info.fd, &(nodes->count), size ) != size )
	{
		ret = errno;
		goto fail;
	}
	
	if ( ret != 0 )
	{
		fail:
		FAILED( ret, pof );
	}
	
	return 0;
}

int dump_files_reset_offsets( dump_t *dump, bool read_info )
{	
	int ret = 0;
	char *pof;
	nodes_t *nodes;
	space_t *space;
	(void)read_info;
	
	pof = "dump object check";
	if ( !dump )
	{
		ret = EDESTADDRREQ;
		goto fail;
	}
	
	dump->done = 0;
	dump->base = 0;
	dump->addr = 0;
	dump->kept = 0;
	dump->size = 0;
	dump->number = 0;
	
	pof = "mappings list check";
	if ( !(nodes = dump->nodes) )
	{
		ret = EINVAL;
		goto fail;
	}
	
	space = &(nodes->space);
	
	(void)memset( dump->used.data, 0, DUMP_LOC_NODE_SIZE );
	(void)memset( dump->data.data, 0, DUMP_LOC_NODE_SIZE );
	(void)memset( space->block, 0, space->given );
	
	pof = "seek to start of used address bit booleans";
	if ( gasp_lseek( dump->used.fd, 0, SEEK_SET ) != 0 ) {
		ret = errno;
		goto fail;
	}
	
	pof = "seek to start of dumpped data";
	if ( gasp_lseek( dump->data.fd, 0, SEEK_SET ) != 0 )
	{
		ret = errno;
		goto fail;
	}
	
	if ( ret != 0 )
	{
		fail:
		FAILED( ret, pof );
	}
	
	return ret;
}

void dump_files_shut( dump_t *dump ) {
	if ( !dump )
		return;
	REPORT("Shutting info file")
	dump_files__shut_file( &(dump->info) );
	REPORT("Shutting used pointers file")
	dump_files__shut_file( &(dump->used) );
	REPORT("Shutting data file")
	dump_files__shut_file( &(dump->data) );
	REPORT("Clearing dump file object of data that can crash execution")
	(void)dump_files_init( dump );
}

int dump_files_test( dump_t dump ) {
	if ( dump.info.fd <= 0 || dump.used.fd <= 0 || dump.data.fd <= 0 )
		return EBADF;
	return 0;
}

typedef uintmax_t (*func_process_rdwr_t)(
	int *err, process_t *process,
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
	int ret = 0;
	char *pof;
	struct pollfd pfd = {0};
	
	pof = "external pipes check";
	if ( ext_pipes[1] <= 0 )
	{
		ret = EINVAL;
		goto fail;
	}
	
	pfd.fd = ext_pipes[1];
	pfd.events = POLLOUT;
	if ( poll( &pfd, 1, msTimeout ) != 1 )
	{
		ret = errno;
		goto fail;
	}
	
	if ( write( ext_pipes[1], &sig, sizeof(int) ) != sizeof(int) )
	{
		ret = errno;
		goto fail;
	}
		
	if ( ret != 0 )
	{
		fail:
		FAILED( ret, pof );
	}
	
	return 1;
}

int gasp_thread_should_die( int ext_pipes[2], int int_pipes[2] )
{
	int ret = SIGCONT;
	if ( !gasp_tpolli( int_pipes, &ret, 0 ) )
		return 0;
	if ( ret == SIGKILL )
		return ECANCELED;
	while ( ret == SIGSTOP )
	{
		if ( !gasp_tpollo( ext_pipes, SIGCONT, -1 ) )
			return ECANCELED;
		if ( !gasp_tpolli( int_pipes, &ret, -1 ) )
			return ECANCELED;
	}
	return (ret == SIGKILL) ? ECANCELED : 0;
}

void dump_files__puts( dump_t *dump )
{
	char *pfx = "dump->";
	if ( !dump ) return;
	EOUTF( "%sinfo.fd = %d, %sdata.fd = %d, %sused.fd = %d",
		pfx, dump->info.fd, pfx, dump->data.fd, pfx, dump->used.fd );
}

node_t process_dump( tscan_t *tscan )
{
	int ret = 0;
	char *pof;
	nodes_t nodes = {0}, *mappings;
	mapped_t *src, *SRC, *dst, *DST;
	process_t *process;
	uchar *used, *data;
	dump_t *dump;
	uintmax_t i, bytes;
	ssize_t size;
	mode_t prot = 04;
	
	if ( !tscan )
		return EDESTADDRREQ;
	
	dump = tscan->dump + 1;
	mappings = dump->nodes;
	used = dump->used.data;
	data = dump->data.data;
	tscan->last_found = 0;
	tscan->done_upto = 0;
	prot |= tscan->writeable ? 02 : 0;
	process = tscan->process;
	
	pof = "process handle check";
	if ( !process )
	{
		ret = EINVAL;
		goto fail;
	}
	
	pof = "dump file desciptor tests";
	if ( (ret = dump_files_test( *dump )) != 0 )
		goto fail;
	
	if ( tscan->zero != 0 )
		tscan->zero = ~0;
	
	/* No further need to change anything */
	pof = "reset dump file offsets";
	if ( (ret = dump_files_reset_offsets( dump, 0 )) != 0 )
		goto fail;
	
	/* Since this is a dump set all address booleans to true,
	 * a later scan can simply override the values,
	 * we use ~0 now because it's bit scan friendly,
	 * not that we're doing those yet */
	(void)memset( dump->used.data, ~0, DUMP_LOC_NODE_SIZE );
	
	if ( tscan->done_scans )
		goto dump_data;
	
	pof = "mappings list";
	if ( (ret = mapped_list( process, &nodes )) != 0 && ret != EOF )
		goto fail;
	
	pof = "allocation for mappings count";
	if ( (ret = more_nodes( mapped_t, mappings, nodes.count )) != 0 )
		goto fail;
	
	DST = mappings->space.block;
	SRC = nodes.space.block;

	mappings->focus = 0;
	mappings->count = 0;
	
	for ( nodes.focus = 0; nodes.focus < nodes.count; nodes.focus++ )
	{
		src = SRC + nodes.focus;
		tscan->done_upto = src->head;
		
		/* Not interested in out of range memory */
		if ( src->foot <= tscan->from || src->head >= tscan->upto )
			continue;		
			
		/* Get focused node, total is at least the same as source
		 * so no need to check if can add since the main loop check
		 * does that for us */
		dst = DST + (dump->nodes->count);
		
		/* Add the mapping to the list */
		(void)memcpy( dst, src, sizeof(mapped_t) );
		dump->nodes->count++;
	}
	
	dump_data:
	
	SRC = DST;
	REPORT( "Dumping mappings in range" )
	for ( mappings->focus = 0;
		mappings->focus < mappings->count; mappings->focus++
	)
	{
		src = SRC + mappings->focus;
		
		for (
			bytes = 0;
			bytes < src->size;
			bytes += size
		)
		{
			i = src->size - bytes;
			if ( i > DUMP_LOC_NODE_SIZE ) i = DUMP_LOC_NODE_SIZE;
			
			pof = "die signal check";
			if ( (ret = gasp_thread_should_die(
				tscan->main_pipes, tscan->scan_pipes )) != 0
			) goto fail;
			
			tscan->done_upto = src->head + bytes;
			
			/* Prevent unread memory from corrupting locations found */
			(void)memset( data, tscan->zero, i );
			
			if ( (ret = pglance_data(
				process, src->head, data, i, &size )) != 0
			) break;
			
			pof = "write data";
			errno = 0;
			if ( gasp_write( dump->data.fd, data, size ) != size )
			{
				ret = errno;
				goto done;
			}
		
			errno = 0;
			if ( gasp_write( dump->used.fd, used, size ) != size )
			{
				ret = errno;
				goto done;
			}
			
			fsync( dump->used.fd );
			fsync( dump->data.fd );
		}
	}
	
	done:
	dump->number = tscan->done_scans;
	ret = dump_files_change_info(dump);
	
	fsync( dump->info.fd );
	
	if ( ret != 0 )
	{
		fail:
		FAILED( ret, pof );
		dump_files__puts( dump );
	}
	
	/* Make sure we don't leak memory */
	free_nodes( &nodes );
	return ret;
}

enum {
	PROC_MAPPED_PTR_VEC = 0,
	PROC_MAPPED_PTR_PRV,
	PROC_MAPPED_PTR_NOW,
	PROC_MAPPED_PTR_COUNT
};

int dump_files__mappings( dump_t *dump, mapped_t **ptrs )
{
	int ret = 0;
	nodes_t *nodes;
	mapped_t *vec, *now;
	char *pof;
	
	pof = "mapping destination check";
	if ( !ptrs )
	{
		ret = EDESTADDRREQ;
		goto fail;
	}
	
	(void)memset( ptrs, 0, PROC_MAPPED_PTR_COUNT * sizeof(void*) );
	
	pof = "dump pointer check";
	if ( !dump )
	{
		ret = EINVAL;
		goto fail;
	}
		
	nodes = dump->nodes;
	
	vec = (mapped_t*)(nodes->space.block);
	
	pof = "mapping nodes pointer check";
	if ( !vec )
	{
		ret = ENODATA;
		goto fail;
	}
	
	ptrs[PROC_MAPPED_PTR_VEC] = vec;
	
	if ( nodes->focus >= nodes->total )
		/* Easier to check against for multi-source handlers */
		return EOF;
	
	now = vec + nodes->focus;
	
	ptrs[PROC_MAPPED_PTR_PRV] = (now != vec) ? now - 1 : NULL;
	ptrs[PROC_MAPPED_PTR_NOW] = now;
	
	if ( ret != 0 )
	{
		fail:
		FAILED( ret, pof );
	}
	
	return ret;
}

int dump_files_glance_stored( dump_t *dump, size_t keep )
{
	int ret = 0;
	char *pof;
	ssize_t expect = 0, size = 0;
	uchar *data, *used;
	mapped_t *pmap, *nmap, *ptrs[PROC_MAPPED_PTR_COUNT] = {NULL};
	nodes_t *nodes;
	
	pof = "dump mappings list check";
	if ( (ret = dump_files__mappings( dump, (mapped_t**)(&ptrs) )) != 0 )
	{
		if ( ret == EINVAL )
			ret = EDESTADDRREQ;
		goto fail;
	}
	
	/* Initialise pointers */
	used = dump->used.data;
	data = dump->data.data;
	
	nodes = dump->nodes;
	nmap = ptrs[PROC_MAPPED_PTR_NOW];
	pmap = ptrs[PROC_MAPPED_PTR_PRV];
	
	dump->kept = 0;
	
	/* Update pointers to change region */
	if ( dump->done == nmap->size )
	{
		dump->done = 0;
		dump->addr = nmap->foot;
		nodes->focus++;
		
		if ( nodes->focus == nodes->total )
			return EOF;

		pmap = ++nmap - 1;
		dump->addr = nmap->head;
	}
	
	expect = nmap->size - dump->done;
	if ( expect > DUMP_LOC_NODE_HALF )
		expect = DUMP_LOC_NODE_HALF;
	dump->base = DUMP_LOC_NODE_HALF;
	dump->last = DUMP_LOC_NODE_HALF;
	
	(void)memmove( used,
		used + DUMP_LOC_NODE_HALF, DUMP_LOC_NODE_HALF );
	(void)memmove( data,
		data + DUMP_LOC_NODE_HALF, DUMP_LOC_NODE_HALF );
	
	pof = "read used pointers file";
	if ( (ret = dump_files__read_file(
		&(dump->used), used + DUMP_LOC_NODE_HALF, expect, &size)) != 0
	)
	{
		fprintf( stderr,
			"(used.fd) 0x%jX - 0x%jX (0x%jX bytes, 0x%jX read), "
			"fd = %d, ptr = %p, size = %zd\n, expect = %zd",
			nmap->head, nmap->foot, nmap->size, dump->done,
			dump->used.fd, used + DUMP_LOC_NODE_HALF, size, expect );
		goto fail;
	}
	
	pof = "read data file";
	if ( (ret = dump_files__read_file(
		&(dump->data), data + DUMP_LOC_NODE_HALF, expect, &size)) != 0
	)
	{
		fprintf( stderr,
			"(data.fd) 0x%jX - 0x%jX (0x%jX bytes, 0x%jX read), "
			"fd = %d, ptr = %p, size = %zd\n, expect = %zd",
			nmap->head, nmap->foot, nmap->size, dump->done,
			dump->data.fd, data + DUMP_LOC_NODE_HALF, size, expect );
		goto fail;
	}
	
	dump->tail = DUMP_LOC_NODE_SIZE - size;
	dump->last = dump->tail - keep;
	
	if ( dump->done )
		dump->addr = nmap->head + dump->done;
	dump->size = size;
	
	if ( dump->done || dump->addr == pmap->foot )
	{
		pof = "keep bytes check";
		if ( keep >= DUMP_LOC_NODE_HALF ) {
			ret = ERANGE;
			goto fail;
		}
		
		dump->addr -= keep;
		dump->kept = keep;
		dump->base -= DUMP_LOC_NODE_HALF;
	}
	dump->done += size;
	
	if ( ret != 0 )
	{
		fail:
		FAILED( ret, pof );
		if ( ret == ENOMEM )
			fprintf( stderr, "Requested 0x%08jX bytes\n", size );
	}
	
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

int process__aobscan_next( tscan_t *tscan, dump_t *dump )
{
	if ( !tscan )
		return EDESTADDRREQ;
	if ( gasp_thread_should_die(
		tscan->main_pipes, tscan->scan_pipes ) )
		return ECANCELED;
	return dump_files_glance_stored( dump, tscan->bytes - 1 );
}

int process__aobscan_test( tscan_t *tscan )
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

int process_aobscan( tscan_t *tscan ) {
	int ret = 0;
	uintmax_t minus;
	mode_t prot = 04;
	bool have_prev_results = 0;
	dump_t *dump, *prev;
	ssize_t done;
	char *pof;
	
	pof = "thread object check";
	if ( !tscan )
	{
		ret = EDESTADDRREQ;
		goto fail;
	}
	
	minus = tscan->bytes - 1;
	tscan->found = 0;
	tscan->done_upto = 0;
	prev = tscan->dump;
	dump = prev + 1;
	
	pof = "limit for bytes check";
	if ( minus > tscan->bytes || tscan->bytes >= DUMP_LOC_NODE_HALF )
	{
		ret = EINVAL;
		goto set_found;
	}

	pof = "dump files check";
	if ( (ret = dump_files_test( *dump )) != 0 )
		goto set_found;

	prot |= tscan->writeable ? 02 : 0;
	
	pof = "rwvmem check";
	if ( (ret = proc__rwvmem_test(
		tscan->process, tscan->array, tscan->bytes, &done )) != 0
	) goto set_found;
	
	pof = "reset dump file offsets";
	if ( (ret = dump_files_reset_offsets( dump, 1 )) != 0 )
	{
		fprintf( stderr, "Failed at dump_files_reset_offsets()\n" );
		goto set_found;
	}
	
	if ( dump_files_test( *prev ) == 0 ) {
		
		pof = "reset previous dump file offsets";
		if ( (ret = dump_files_reset_offsets( prev, 1 )) != 0 )
			goto set_found;
		
		have_prev_results = 1;
	}
	
	/* Make sure we start at the first recorded mapping */
	dump->nodes->focus = 0;

	REPORTF( "Started scan at %p", (void*)(dump->addr) )

	while ( (ret = process__aobscan_next( tscan, dump )) == 0 )
	{
		REPORTF( "%p-%p",
			(void*)(dump->addr), (void*)(dump->addr + dump->size) )
		
		if ( have_prev_results ) {
			/* Ensure we check only pointers that have been marked
			 * as a result previously */
			pof = "read previous dump file";
			if (
				(ret = dump_files_glance_stored(
					prev, tscan->bytes - 1 )) != 0
			) goto set_found;
			
			(void)memcpy(
				dump->used.data, prev->used.data, DUMP_LOC_NODE_SIZE );
		}
		else {
			/* Ensure we check all pointers */
			(void)memset( dump->used.data, ~0, DUMP_LOC_NODE_SIZE );
		}
		
		if ( !process__aobscan_test( tscan ) )
			goto set_found;
	}
	
	REPORTF( "Ended scan at %p", (void*)(dump->addr) )
	
	tscan->done_upto = tscan->upto;
	
	set_found:
	tscan->last_found = tscan->found;
	tscan->done_scans++;
	
	pof = "result check";
	switch ( ret ) { case EOF: case ECANCELED: ret = 0; }
	
	if ( ret != 0 )
	{
		fail:
		FAILED( ret, pof );
	}
	
	return ret;
}

void* process_dumper( void * _tscan ) {
	int ret = 0;
	tscan_t *tscan = _tscan;
	char *pof;
	
	pof = "thread object check";
	if ( !tscan )
	{
		ret = EDESTADDRREQ;
		goto fail;
	}
		
	tscan->threadmade = 1;
	tscan->found = 0;
	
	REPORT( "Starting dump" )

	tscan->dumping = 1;
	ret = process_dump( tscan );
	tscan->dumping = 0;
	
	pof = "dump result check";
	if ( ret != 0 )
		goto fail;

#if VERBOSE
	fprintf( stderr, "Starting scan" );
#endif
	
	tscan->scanning = 1;
	ret = process_aobscan( tscan );
	tscan->scanning = 0;
	
	pof = "scan result check";
	if ( ret != 0 )
		goto fail;

	REPORT( "Finished" )
	
	gasp_tpollo( tscan->main_pipes, SIGCONT, -1 );
	tscan->threadmade = 0;
	tscan->ret = ret;
	if ( ret != 0 )
	{
		fail:
		FAILED( ret, pof );
	}
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

int fill_text_object_with_str( text_t *nodes, char const *txt ) {
	int ret;
	size_t size = strlen(txt);
	
	if ( (ret = more_nodes( char, nodes, size + 1 )) != 0 )
		return ret;

	nodes->focus = 0;
	nodes->count = size;
	(void)memcpy( nodes->space.block, txt, size );
	return 0;
}

int process_find(
	char const *name, nodes_t *nodes, int underId, bool usecase )
{
	int ret = 0;
	node_t i = 0;
	pglance_t glance = {0};
	process_t *process, *processes, *ent;
	char *text, *pof;
	char const * (*instr)( char const *src, char const *txt )
		= usecase ? substr : substri;
	
	pof = "nodes pointer check";
	if ( !nodes ) {
		ret = EDESTADDRREQ;
		goto fail;
	}
	
	(void)memset( nodes, 0, sizeof(nodes_t) );
	
	process = &(glance.process);
	for ( ret = pglance_init( &glance, underId )
		; ret == 0; ret = process_next( &glance )
	)
	{	
		if ( name && *name ) {
			text = process->name.space.block;
			if ( !instr( text, name ) ) {
				text = process->cmdl.space.block;
				if ( !instr( text, name ) )
					continue;
			}
		}
		
		if ( (ret = add_node( nodes, &i, sizeof(process_t) )) != 0 )
			break;
			
		processes = nodes->space.block;
		ent = processes + i;
		
		if ( (ret = fill_text_object_with_str(
			&(ent->name), process->name.space.block )) != 0
		)
		{
			nodes->count--;
			break;
		}
		
		if ( (ret = fill_text_object_with_str(
			&(ent->cmdl), process->cmdl.space.block )) != 0
		)
		{
			nodes->count--;
			break;
		}
	}
	
	pglance_term( &glance );
	if ( nodes->count > 0 )
		return 0;
	if ( ret == 0 )
		ret = ENOENT;
	if ( ret != ENONET )
	{
		pof = "Final process in glance check";
		fail:
		FAILED( ret, pof );
	}
	return ret;
}

int process__init( process_t *process )
{
	if ( !process )
		return EDESTADDRREQ;
		
	(void)memset( process, 0, sizeof(process_t) );
	process->thread.tid = -1;
	process->parent = -1;
	process->pid = -1;
	
	return 0;
}

void process_term( process_t *process ) {
	if ( !process ) return;
	
	/* Ensure thread closes before we release the handle it's using */
	if ( process->thread.active )
		process->thread.active = 0;
	
	process_free( process );
	process_cont( process );
	
	(void)change_kvpair( &(process->kvpair), 0, 0, 0, 0 );
	free_nodes( &(process->name) );
	free_nodes( &(process->cmdl) );
	
	/* Should be last */
	(void)process__init(process);
}

void pglance_term( pglance_t *glance ) {
	if ( !glance ) return;
	free_nodes( &(glance->nodes) );
	process_term( &(glance->process) );
	(void)memset( glance, 0, sizeof(pglance_t) );
}

void process_data_state_puts( process_t *process ) {
	if ( !process )
		return;
	fprintf( stderr, "%s() Process %04X under %04X is named '%s'\n",
		__func__, process->pid, process->parent,
		(char*)(process->name.space.block) );
}

int process_info( process_t *process, int pid, bool hook, int flags )
{
	int ret = 0, fd = -1;
	char path[bitsof(int) * 2] = {0};
	text_t *name, *cmdl;
	space_t *full;
	char *k, *v, *n, *pof;
	intmax_t size;
	
	pof = "process pointer check";
	if ( !process )
	{
		ret = EDESTADDRREQ;
		goto fail;
	}
	
	pof = "handle activity check";
	if ( process->hooked || process->thread.active )
	{
		ret = EBUSY;
		goto fail;
	}
	
	/* Set pointers */
	name = &(process->name);
	cmdl = &(process->cmdl);
	full = &(process->kvpair.full);
	
	/* Fill core information */
	(void)memset( &(process->thread), 0, sizeof(proc_thread_t) );
	process->thread.tid = -1;
	process->parent = -1;
	process->hooked = 0;
	process->paused = 0;
	process->exited = 0;
	process->flags = 0;
	process->pid = pid;
	
	/* Fill base path */
	if ( (process->self = (pid == getpid())) )
		sprintf( process->path, "/proc/self" );
	else
		sprintf( process->path, "/proc/%d", pid );
	
	/* Clear invalid text */
	if ( name->space.block )
		(void)memset( name->space.block, 0, name->space.given );
	if ( cmdl->space.block )
		(void)memset( cmdl->space.block, 0, cmdl->space.given );
	if ( full->block )
		(void)memset( full->block, 0, full->given );
	
	/* Prevent attempts to read system process */
	pof = "pid check";
	if ( pid < 1 )
	{
		ret = EDESTADDRREQ;
		goto fail;
	}
	
	sprintf( path, "%s/cmdline", process->path );
	
	pof = "sizeof /proc/<PID>/cmdline";
	if ( (ret = file_glance_size( path, &size )) != 0 )
		goto fail;
	
	pof = "/proc/<PID>/cmdline destination allocation";
	if ( (ret = more_nodes( char, cmdl, size )) != 0 )
		goto fail;
	
	pof = "open /proc/<PID>/cmdline";
	errno = 0;
	if ( (fd = open(path,O_RDONLY)) < 0 )
	{
		ret = errno;
		goto fail;
	}
	
	pof = "read /proc/<PID>/cmdline";
	errno = 0;
	if ( gasp_read( fd, cmdl->space.block, cmdl->space.given ) < size )
	{
		ret = errno;
		goto fail;
	}
	
	/* No longer need the file opened since we have everything */
	close(fd);
	fd = -1;
	
	sprintf( path, "%s/status", process->path );
	
	pof = "sizeof /proc/<PID>/status";
	if ( (ret = file_glance_size( path, &size )) != 0 )
		goto fail;
	
	pof = "/proc/<PID>/status destination allocation";
	if ( (ret = more_space( full, size * 2 )) != 0 )
		goto fail;
	
	pof = "open /proc/<PID>/status";
	errno = 0;
	if ( (fd = open(path,O_RDONLY)) < 0 )
	{
		ret = errno;
		goto fail;
	}
	
	pof = "read /proc/<PID>/status";
	errno = 0;
	if ( gasp_read( fd, full->block, full->given ) <= 0 )
	{
		ret = errno;
		goto fail;
	}
	
	/* No longer need the file opened since we have everything */
	close(fd);
	fd = -1;
	
	/* Ensure we have enough space for the name */
	pof = "name allocation";
	if ( (ret = more_nodes( char, name, full->given )) != 0 )
		goto fail;
	
	k = full->block;
	while ( size > 0 )
	{
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
			memcpy( name->space.block, v + 2, strlen(v+1) );
		if ( strcmp( k, "PPid" ) == 0 )
			process->parent = strtol( v + 2, NULL, 10 );
		*v = ':';
		if ( n )
			*n = '\n';
		k = ++n;
	}
	
	pof = "name pointer and ppid check";
	if ( !(*((char*)(name->space.block))) || process->parent < 0 )
		goto fail;
	
	if ( hook )
	{
		process->flags = flags;
		pof = "wait4exit thread creation";
		errno = 0;
		if ( pthread_create( &(process->thread.thread), NULL,
			process__wait4exit, process ) != 0
		)
		{
			ret = errno;
			goto fail;
		}
	}

	if ( ret != 0 )
	{
		fail:
		FAILED( ret, pof );
		process_data_state_puts(process);
	}
	
	if ( fd > -1 )
		close(fd);
	return ret;
}

int	process_next( pglance_t *glance )
{
	int ret = 0;
	process_t *process;
	int pid = 0, *pids;
	nodes_t *nodes;
	
	if ( !glance )
		return EDESTADDRREQ;

	nodes = &(glance->nodes);
	process = &(glance->process);
	pids = (int*)(nodes->space.block);
	for ( ; nodes->focus < nodes->count; nodes->focus++ )
	{
		pid = process->parent = pids[nodes->focus];

		while ( (ret = process_info(
			process, process->parent, 0, 0 )) == 0
		)
		{
			if ( process->parent == glance->underId )
			{
				nodes->focus++;
				return process_info( process, pid, 0, 0 );
			}
			
			if ( process->parent <= 0 )
				break;
		}
		if ( ret != 0 )
			return ret;
	}
	
	pglance_term( glance );
	return ENODATA;
}

int	pglance_init( pglance_t *glance, int underId )
{
	int ret = errno = 0;
	DIR *dir;
	struct dirent *ent;
	node_t i = 0;
	char *name, *pof;
	
	pof = "glance pointer check";
	if ( !glance )
	{
		ret = EDESTADDRREQ;
		goto fail;
	}
	
	(void)memset( glance, 0, sizeof(pglance_t) );
	
	pof = "glance pid buffer allocation";
	glance->underId = underId;
	if ( (ret = more_nodes( int, &(glance->nodes), 2000 )) == 0 )
	{
		pof = "open directory";
		errno = 0;
		if ( (dir = opendir("/proc")) ) {
			while ( (ent = readdir( dir )) ) {
				for ( name = ent->d_name;; ++name ) {
					if ( *name < '0' || *name > '9' )
						break;
				}
				if ( *name )
					continue;
				
				if ( (ret = add_node(
					&(glance->nodes), &i, sizeof(int) ))
					!= 0 )
					break;

				sscanf( ent->d_name, "%d",
					((int*)(glance->nodes.space.block)) + i
				);
			}
			closedir(dir);
			
			pof = "pid count check";
			if ( glance->nodes.count )
				return process_next( glance );
			ret = EPERM;
		}
		else ret = errno;
	}
	
	fail:
	FAILED( ret, pof );
	return ret;
}

int pglance_data(
	process_t *process,
	uintmax_t addr, void *mem, ssize_t size, ssize_t *done
)
{
	int ret = 0;
	char *pof;
	
	pof = "rwvmem check";
	if ( (ret = proc__rwvmem_test( process, mem, size, done )) != 0 )
		goto fail;
	
	pof = "stop target process";
	if ( (ret = process_stop(process)) != 0 )
		goto fail;
	
	pof = "grab process handle";
	if ( (ret = process_grab(process)) != 0 ) 
		goto fail;
	
	pof = "glance via memmove";
	if ( (ret =
		proc__glance_self( process, addr, mem, size, done )) == 0
	) goto done;
#if 1
	pof = "glance via file (pread)";
	if ( (ret =
		proc__glance_file( process, addr, mem, size, done )) == 0
	) goto done;
#endif
#if 1
	pof = "glance via file (lseek, read)";
	if ( (ret =
		proc__glance_seek( process, addr, mem, size, done )) == 0
	) goto done;
#endif
#if 0
	pof = "glance via process_vm_readv";
	if ( (ret =
		proc__glance_vmem( process, addr, mem, size, done )) == 0
	) goto done;
#endif
	pof = "glance via ptrace";
	if ( (ret =
		proc__glance_peek( process, addr, mem, size, done )) == 0
	) goto done;
	
	/* Detatch and ensure continued activity */
	done:
	ret = process_free(process);
	ret = process_cont(process);
	if ( ret != 0 )
	{
		fail:
		FAILED( ret, pof );
	}
	return ret;
}

int proc_change_data(
	process_t *process,
	uintmax_t addr, void *mem, ssize_t size, ssize_t *done
)
{
	int ret;

	if ( (ret = proc__rwvmem_test(process,mem,size,done)) != 0 )
		return ret;
	
	if ( (ret = process_stop(process)) != 0 )
		return ret;

	if ( (ret = process_grab(process)) != 0 )
		return ret;
	
	/* Avoid fiddling with own memory file */
	if ( (ret =
		proc__change_self( process, addr, mem, size, done )) == 0
	) goto success;
	/* gasp_pwrite() */
	if ( (ret =
		proc__change_file( process, addr, mem, size, done )) == 0
	) goto success;
	/* gasp_seek() & gasp_write() */
	if ( (ret =
		proc__change_seek( process, addr, mem, size, done )) == 0
	) goto success;
	/* Last ditch effort to avoid ptrace */
	if ( (ret =
		proc__change_vmem( process, addr, mem, size, done )) == 0
	) goto success;
	/* PTRACE_POKEDATA */
	if ( (ret =
		proc__change_poke( process, addr, mem, size, done )) == 0
	) goto success;
	
	success:
	ret = process_free(process);
	return process_cont(process);
}
