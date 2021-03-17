#include <pipes.h>

#ifdef _WIN32

int open_pipes( pipe_t *pipes )
{
	int ret;
	SECURITY_ATTRIBUTES sa;
	sa.nLength = sizeof(sa);
	sa.lpSecurityDescriptor = NULL;
	sa.bInheritHandle = true;

	ret = (CreatePipe( &pipes[PIPE_RD], &pipes[PIPE_WR], 0 ) != 0 );
	
	if ( ret != 0 )
	{
		pipes[0] = pipes[1] = INVALID_PIPE;
	}
	
	return ret;
}

void shut_pipes( pipe_t *pipes )
{
	CloseHandle( pipes[PIPE_WR] );
	CloseHandle( pipes[PIPE_RD] );
	
	pipes[0] = pipes[1] = INVALID_PIPE;
}

int pipe_err( pipe_t pipe )
{
	return ENOSYS;
}

int poll_pipe( pipe_t pipe )
{
	/* Not supported yet */
	return -1;
}

int rdpipe( pipe_t pipe, void *data, ssize_t *done )
{
	DWORD bytes = 0;
	
	#error Should catch any errors returned by this
	ReadFile( pipe, data, sizeof(void*), &bytes, NULL );
	
	*done = bytes;
	
	return 0;
}

int wrpipe( pipe_t pipe, void *data, ssize_t *done )
{
	DWORD bytes = 0;
	
	#error Should catch any errors returned by this
	WriteFile( pipe, data, sizeof(void*), &bytes, NULL );
	
	*done = bytes;
	
	return 0;
}

#else

int open_pipes( pipe_t *pipes )
{
	pipes[0] = pipes[1] = INVALID_PIPE;
	
	return pipe( pipes );
}

void shut_pipes( pipe_t *pipes )
{
	close( pipes[PIPE_WR] );
	close( pipes[PIPE_RD] );
	
	pipes[0] = pipes[1] = INVALID_PIPE;
}

int pipe_err( pipe_t pipe )
{
	return errno;
}

int poll_pipe( pipe_t pipe )
{
	struct pollfd fds;
	fds.fd = pipe;
	fds.events = POLLIN | POLLERR | POLLHUP | POLLNVAL;
	return poll( &fds, 1, 1 );
}

int rdpipe( pipe_t pipe, void *data, ssize_t *done )
{
	int ret;
	ssize_t byte = 0;
	char *dst = (void*)(&data), tmp[sizeof(void*)];
	
	while ( byte < sizeof(void*) )
	{
		int ret;
		ssize_t get = sizeof(void*) - byte;
		ssize_t bytes = read( pipe, data, get );
		#pragma message "Should consider using a mutex here as this is shared"
		ret = errno;
		
		for ( ssize_t b = 0; b < bytes; dst[byte] = tmp[b], ++b, ++byte );
		
		if ( bytes < get && ret != 0 && ret != EINTR )
		{
			*done = byte;
			return ret;
		}
	}
	
	*done = byte;
	return 0;
}

int wrpipe( pipe_t pipe, void *data, ssize_t *done )
{
	int ret;
	ssize_t byte = 0;
	char *dst = (void*)(&data), tmp[sizeof(void*)];
	
	while ( byte < sizeof(void*) )
	{
		int ret;
		ssize_t set = sizeof(void*) - byte;
		ssize_t bytes = write( pipe, data, set );
		#pragma message "Should consider using a mutex here as this is shared"
		ret = errno;
		
		for ( ssize_t b = 0; b < bytes; dst[byte] = tmp[b], ++b, ++byte );
		
		if ( bytes < set && ret != 0 && ret != EINTR )
		{
			*done = byte;
			return ret;
		}
	}
	
	*done = byte;
	return 0;
}

#endif
