#include "gasp.h"

size_t file_get_size( int *err, char const *path ) {
	int fd;
	gasp_off_t size;
	errno = EXIT_SUCCESS;
	if ( (fd = open(path,O_RDONLY)) < 0 ) {
		if ( err ) *err = errno;
		return 0;
	}
	size = gasp_lseek( fd, 0, SEEK_END );
	close(fd);
	if ( size <= 0 ) {
		if ( err ) *err = errno;
		return 0;
	}
	return size;
}
bool proc_has_died( int *err, int pid ) {
	char path[256];
	(void)sprintf( path, "/proc/%d/cmdline", pid );
	return !file_get_size(err,path);
}
proc_handle_t* proc_handle_open( int *err, int pid ) {
	char path[256] = {0};
	key_val_t key_val = {{0}};
	proc_handle_t *handle;
	long attach_ret = -1;
	int ptrace_mode = (PTRACE_SEIZE) ? (PTRACE_SEIZE) : -1;
	if ( pid != getpid() ) {
		if ( ptrace_mode > 0 )
			attach_ret = ptrace( ptrace_mode, pid, NULL, NULL );
		if ( attach_ret != 0 ) { 
			if ( ptrace_mode == -1 ) {
				if ( err ) *err = ENOSYS;
				puts("PTRACE_SEIZE not supported");
			}
			else if ( err ) *err = errno;
			return NULL;
		}
	}
	if ( !(handle = calloc(sizeof(proc_handle_t),1)) ) {
		if ( err ) *err = errno;
		return NULL;
	}
	if ( !proc_notice_info( err, pid, &key_val, &(handle->notice) ) ) {
		less_space( NULL, &(key_val.full), 0 );
		less_space( NULL, &(key_val.key), 0 );
		less_space( NULL, &(key_val.val), 0 );
		free(handle);
		return NULL;
	}
	less_space( NULL, &(key_val.full), 0 );
	less_space( NULL, &(key_val.key), 0 );
	less_space( NULL, &(key_val.val), 0 );
	(void)sprintf( path, "/proc/%d/maps", pid );
	if ( (handle->pagesFd = open(path,O_RDONLY)) < 0 ) {
		free(handle);
		if ( err ) *err = errno;
		return NULL;
	}
	(void)sprintf( path, "/proc/%d/mem", pid );
	if ( (handle->rdMemFd = open(path,O_RDONLY)) < 0 ) {
		close( handle->pagesFd );
		free(handle);
		if ( err ) *err = errno;
		return NULL;
	}
	handle->wrMemFd = open(path,O_WRONLY);
	return handle;
}
void proc_handle_shut( proc_handle_t *handle ) {
	if ( !handle ) return;
	proc_notice_none( &(handle->notice) );
	if ( handle->rdMemFd >= 0 ) close( handle->rdMemFd );
	if ( handle->wrMemFd >= 0 ) close( handle->wrMemFd );
	if ( handle->pagesFd >= 0 ) close( handle->pagesFd );
	(void)memset(handle,0,sizeof(proc_handle_t));
	free(handle);
}

#define PAGE_LINE_SIZE ((sizeof(void*) * 4) + 7)

intptr_t proc_mapped_next( int *err, proc_handle_t *handle, intptr_t addr, proc_mapped_t *mapped ) {
	char line[PAGE_LINE_SIZE] = {0}, *perm;
	intptr_t from = 0, upto = 0;
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
		if ( addr >= upto ) {
			while ( gasp_read( handle->pagesFd, line, 1 ) == 1 ) {
				if ( line[0] == '\n' )
					break;
			}
			if ( line[0] != '\n' ) {
				if ( err ) *err = errno;
				return -1;
			}
		}
	} while ( addr >= upto );
	perm = strstr( line, " " );
	++perm;
	mapped->size = (mapped->upto) - (mapped->base);
	mapped->perm |= (perm[0] == 'r') ? 04 : 0;
	mapped->perm |= (perm[1] == 'w') ? 02 : 0;
	mapped->perm |= (perm[2] == 'x') ? 01 : 0;
	return from;
}

int proc_mapped_addr(
	int *err, proc_handle_t *handle,
	intptr_t addr, int octal_perm ) {
	proc_mapped_t mapped = {0};
	while ( proc_mapped_next( err, handle, mapped.base, &mapped ) >= 0 ) {
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
	intptr_t from, intptr_t upto ) {
	int ret = EXIT_SUCCESS;
	node_t count = 0;
	uchar buff[BUFSIZ*2] = {0}, *i, *next;
	intptr_t done;
	proc_mapped_t mapped = {0};
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
		else next = buff + (done - bytes) + 1;
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
		if ( from >= mapped.upto ) {
			if ( proc_mapped_next( err, handle, from, &mapped ) < 0 )
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

proc_notice_t*
	proc_locate_name(
	int *err, char *name, nodes_t *nodes, int underId )
{
	size_t i = 0, size;
	proc_glance_t glance;
	proc_notice_t *notice, *noticed;
	char *text;
	if ( !nodes )
		return NULL;
	memset( nodes, 0, sizeof(nodes_t) );
	for ( notice = proc_glance_open( err, &glance, underId )
		; notice; notice = proc_notice_next( err, &glance ) ) {
		text = notice->name.block;
		if ( !text || !(*text) || !strstr( text, name ) )
			continue;
		if ( i == nodes->total && !(noticed = more_nodes(
			proc_notice_t, err, nodes, nodes->total + 10 )) ) {
			proc_glance_shut( &glance );
			return (i > 0) ? nodes->space.block : NULL;
		}
		noticed[i].entryId = notice->entryId;
		if ( !more_space(err,&(noticed[i].name),(size = strlen(text)+1)) ) {
			proc_glance_shut(&glance);
			return (i > 0) ? nodes->space.block : NULL;
		}
		(void)memcpy( noticed[i].name.block, text, size );
		nodes->count = ++i;
	}
	return (i > 0) ? nodes->space.block : NULL;
}

void proc_notice_none( proc_notice_t *notice ) {
	if ( !notice ) return;
	(void)less_space( NULL, &(notice->name), 0 );
}

void proc_glance_shut( proc_glance_t *glance ) {
	if ( !glance ) return;
	proc_notice_none( &(glance->notice) );
	(void)less_space( NULL, &(glance->key_val.full), 0 );
	(void)less_space( NULL, &(glance->key_val.key), 0 );
	(void)less_space( NULL, &(glance->key_val.val), 0 );
	if ( glance->dir ) closedir( glance->dir );
	(void)memset( glance, 0, sizeof(proc_glance_t) );
}

proc_notice_t*
	proc_notice_info(
	int *err, int pid, key_val_t *key_val, proc_notice_t *notice )
{
	FILE *file;
	char path[256];
	space_t *full, *key, *val;
	int ret;
	if ( !key_val || !notice ) {
		if ( err ) *err = EDESTADDRREQ;
		if ( !key_val ) ERRMSG( EDESTADDRREQ, "key_val was NULL" );
		if ( !notice ) ERRMSG( EDESTADDRREQ, "notice was NULL" );
		return NULL;
	}
	notice->entryId = pid;
	sprintf( path, "/proc/%d/status", pid );
	file = fopen(path,"rb");
	if ( !file ) {
		if ( err ) *err = errno;
		ERRMSG( errno, "Couldn't open status file" );
		printf( "File '%s'\n", path );
		return NULL;
	}
	full = &(key_val->full);
	key = &(key_val->key);
	val = &(key_val->val);
	memset( full->block, 0, full->given );
	memset( key->block, 0, key->given );
	memset( val->block, 0, val->given );
	if ( (ret = change_key_val(
		key_val, BUFSIZ, BUFSIZ, BUFSIZ, 1 )) != EXIT_SUCCESS ) {
		if ( err ) *err = ret;
		ERRMSG( ret, "Couldn't allocate memory for key/val pair");
		fclose(file);
		return NULL;
	}
	while ( 1 ) {
		fseek( file, 0, SEEK_SET );
		if ( !fgets(full->block, full->given, file) ) {
			fclose(file);
			return NULL;
		} 
		if ( strstr( full->block, "\n" ) ||
			strlen( full->block ) < full->given - 1 )
			break;
		if ( (ret = change_key_val(
			key_val, full->given + BUFSIZ,
			key->given, val->given, 1 ))
			!= EXIT_SUCCESS ) {
			if ( err ) *err = ret;
			fclose(file);
			return NULL;
		}
	}
	if ( (ret = change_key_val(
		key_val, full->given,
		full->given, full->given, 1 ))
		!= EXIT_SUCCESS ) {
		if ( err ) *err = ret;
		ERRMSG( ret, "Couldn't allocate memory for key/val pair" );
		fclose(file);
		return NULL;
	}
	(void)sscanf( full->block, "%s%*[\t]%s[^\n]",
		(char*)(key->block), (char*)(notice->name.block) );
	fseek( file, 0, SEEK_SET );
	while ( fscanf( file, "%s", (char*)(full->block) ) > 0 ) {
		if ( strcmp( full->block, "PPid:" ) == 0 ) break;
	}
	fscanf( file, "%d",	&(notice->ownerId) );
	if ( err ) *err = EXIT_SUCCESS;
	fclose(file);
	return notice;
}

proc_notice_t*
	proc_notice_next( int *err, proc_glance_t *glance ) {
	struct dirent *ent;
	proc_notice_t *notice, tmp = {0};
	if ( !glance ) {
		if ( err ) *err = EDESTADDRREQ;
		ERRMSG( EDESTADDRREQ, "glance was NULL" );
		return NULL;
	}
	if ( err && *err != EXIT_SUCCESS ) {
		proc_glance_shut( glance );
		return NULL;
	}
	notice = &(glance->notice);
	if ( !(glance->dir) ) {
		if ( err ) *err = EDESTADDRREQ;
		ERRMSG( EDESTADDRREQ, "glance->dir was NULL" );
		proc_glance_shut( glance );
		return NULL;
	}
	next_proc:
	if ( !(ent = readdir(glance->dir)) ) {
		if ( err ) *err = EXIT_SUCCESS;
		proc_notice_none( &tmp );
		proc_glance_shut( glance );
		return NULL;
	}
	if ( ent->d_name[0] < '0' || ent->d_name[0] > '9' ) goto next_proc;
	(void)sscanf( ent->d_name, "%d", &(tmp.entryId) );
	notice->entryId = tmp.entryId;
	while ( proc_notice_info( err,
		tmp.entryId, &(glance->key_val), &tmp ) )
	{
		if ( tmp.ownerId == glance->underId ) {
			proc_notice_none( &tmp );
			return proc_notice_info( err,
			notice->entryId, &(glance->key_val), notice );
		}
		if ( tmp.ownerId == 0 )
			break;
		tmp.entryId = tmp.ownerId;
	}
	goto next_proc;
}

proc_notice_t*
	proc_glance_open( int *err, proc_glance_t *glance, int underId ) {
	int ret = errno = EXIT_SUCCESS;
	if ( !glance ) {
		if ( err ) *err = EDESTADDRREQ;
		ERRMSG( EDESTADDRREQ, "glance was NULL" );
		return NULL;
	}
	(void)memset( glance, 0, sizeof(proc_glance_t) );
	glance->underId = underId;
	glance->dir = opendir("/proc");
	if ( glance->dir )
		return proc_notice_next( err, glance );
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
	off = gasp_lseek( handle->rdMemFd, addr, SEEK_SET );
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
	off = gasp_lseek( handle->wrMemFd, addr, SEEK_SET );
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
