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

ssize_t rdpipe( pipe_t pipe, void *data )
{
	DWORD bytes = 0;
	
	ReadFile( pipe, data, sizeof(void*), &bytes, NULL );
	
	return bytes;
}

ssize_t wrpipe( pipe_t pipe, void *data )
{
	DWORD bytes = 0;
	
	WriteFile( pipe, data, sizeof(void*), &bytes, NULL );
	
	return bytes;
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

ssize_t rdpipe( pipe_t pipe, void *data )
{
	return read( pipe, data, sizeof(void*) );
}

ssize_t wrpipe( pipe_t pipe, void *data )
{
	return write( pipe, data, sizeof(void*) );
}

#endif
