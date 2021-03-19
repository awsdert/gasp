#include <workers.h>

char const * const worker_msg_txt( int msg )
{
	switch ( msg )
	{
	case WORKER_MSG_NONE: return "None";
	case WORKER_MSG_ALLOC: return "Allocation";
	case WORKER_MSG_DIED: return "Died";
	case WORKER_MSG_EXIT: return "Abort everything";
	case WORKER_MSG_RESEND: return "Resend message";
	case WORKER_MSG_TERM: return "Terminate worker";
	case WORKER_MSG_WAIT: return "Wait for main worker";
	case WORKER_MSG_CONT: return "Waiting for target worker";
	}
	return "Unnamed message";
}
