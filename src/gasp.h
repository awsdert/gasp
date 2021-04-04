#ifndef GASP_H
#define GASP_H

#include <workers.h>
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ptrace.h>
#include <sys/wait.h>
#include <sys/uio.h>

#include <dirent.h>

typedef unsigned char uchar;

int lua_dumpstack( lua_State *L );
int lua_panic_cb( lua_State *L );
void push_global_cfunc( lua_State *L, char const *key, lua_CFunction val );
void push_global_bool( lua_State *L, char const *key, bool val );
void push_global_int( lua_State *L, char const *key, int val );
void push_global_num( lua_State *L, char const *key, long double val );
void push_global_obj( lua_State *L, char const *key );
void push_global_str( lua_State *L, char const *key, char *val );
void push_branch_cfunc( lua_State *L, char const *key, lua_CFunction val );
void push_branch_bool( lua_State *L, char const *key, bool val );
void push_branch_int( lua_State *L, char const *key, int val );
void push_branch_num( lua_State *L, char const *key, long double val );
void push_branch_obj( lua_State *L, char const *key );
void push_branch_str( lua_State *L, char const *key, char *val );
void lua_create_gasp(lua_State *L);

#define SIZEOF_PROCESS_BASE_PATH bitsof(int)

typedef struct process {
	bool self, hooked, paused, exited;
	int pid, parent;
	/* Used to internally mimic behaviour if not natively supported */
	mode_t flags;
	//proc_thread_t thread; 
	//text_t name, cmdl;
	//kvpair_t kvpair;
#ifdef _WIN32
	HANDLE handle;
#else
	char path[SIZEOF_PROCESS_BASE_PATH];
#endif
} process_t;

struct glance_at_apps {
	int underId;
	process_t process;
	struct memory_group memory_group;
};

struct glance_at_maps
{
	char *save2dir;
	struct memory_group maps;
	struct memory_group *path;
	struct memory_block *buff;
};

struct mapped {
	mode_t perm, prot;
	char _perm[8];
	int dev_major, dev_minor, inode;
	uintmax_t head, foot, size, moff;
	struct memory_group msrc;
};


#endif
