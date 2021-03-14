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

ssize_t rdpipe( pipe_t pipe, void *data, size_t size )
{
	DWORD bytes = 0;
	
	ReadFile( pipe, data, size, &bytes, NULL );
	
	return bytes;
}

ssize_t wrpipe( pipe_t pipe, void *data, size_t size )
{
	DWORD bytes = 0;
	
	WriteFile( pipe, data, size, &bytes, NULL );
	
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

ssize_t rdpipe( pipe_t pipe, void *data, size_t size )
{
	return read( pipe, data, size );
}

ssize_t wrpipe( pipe_t pipe, void *data, size_t size )
{
	return write( pipe, data, size );
}

#endif
