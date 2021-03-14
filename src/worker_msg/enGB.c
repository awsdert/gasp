#include <workers.h>

char const * const worker_msg_txt( int msg )
{
	switch ( msg )
	{
	case ALL_MSG_NIL: return "ALL_MSG_NIL";
	case ALL_MSG_DIE: return "ALL_MSG_DIE";
	case MM_MSG_NEW: return "MM_MSG_NEW";
	case MM_MSG_DEL: return "MM_MSG_DEL";
	case MM_MSG_ALT: return "MM_MSG_ALT";
	case MM_MSG_INC: return "MM_MSG_INC";
	case MM_MSG_DEC: return "MM_MSG_DEC";
	case MM_MSG_DIE: return "MM_MSG_DIE";
	case MM_MSG_WAIT: return "MM_MSG_WAIT";
	case MM_MSG_READY: return "MM_MSG_READY";
	}
	/* Ensure any unsupported values cause errors at the app end */
	return NULL;
}
