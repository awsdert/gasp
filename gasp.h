#ifndef GASP_H
#define GASP_H

#include "gasp_limits.h"
#include <errno.h>
#include <error.h>
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <inttypes.h>
#include <malloc.h>
#include <ctype.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ptrace.h>
#include <sys/wait.h>
#include <sys/uio.h>
#include <pthread.h>
#include <fcntl.h>
#include <unistd.h>
#include <poll.h>
#include <dirent.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

#define VERBOSE 1
#define COUT( TXT ) fprintf( stdout, "%s:%u:" TXT "\n", __FILE__, __LINE__ )
#define COUTF( F, ... ) fprintf( stdout, "%s:%u:" F "\n", __FILE__, __LINE__, __VA_ARGS__ )
#define EOUT( TXT ) fprintf( stderr, "%s:%u:" TXT "\n", __FILE__, __LINE__ )
#define EOUTF( F, ... ) fprintf( stderr, "%s:%u:" F "\n", __FILE__, __LINE__, __VA_ARGS__ )

#if VERBOSE
#define REPORT(MSG) COUT( " Info: " #MSG );
#define REPORTF(MSG,...) COUTF( " Info: " #MSG, __VA_ARGS__ );
#else
#define REPORT(MSG)
#define REPORTF(MSG,...)
#endif

#define ERRMSG( ERR, MSG ) \
	fprintf( stderr, "%s:%d:%s() 0x%08X"\
	"\nExternal Msg '%s'\nInternal Msg '%s'\n"\
	, __FILE__, __LINE__, __func__, ERR, strerror(ERR), MSG )
#if TOVAL(HAVE_8_BYTE_INT)
#define gasp_off_t off64_t
#define gasp_lseek lseek64
#define gasp_read read64
#define gasp_write write64
#if (_XOPEN_SOURCE) >= 500
#define gasp_pread pread64
#define gasp_pwrite pwrite64
#endif
#else
#define gasp_off_t off_t
#define gasp_lseek lseek
#define gasp_read read
#define gasp_write write
#if (_XOPEN_SOURCE) >= 500
#define gasp_pread pread
#define gasp_pwrite pwrite
#endif
#endif
typedef unsigned char uchar;
typedef unsigned short ushort;
typedef unsigned int uint;
typedef unsigned long ulong;
typedef struct space {
	size_t given;
	void *block;
} space_t;

/** @brief Memory allocater, usually via malloc, realloc & free
 * @param space Desitination of allocated memory, initialise with {0}
 * @param want Amount of memory you want
 * @param dir Direction to restraint allocation to, see below defines
 * @return 0 on success or errno.h code
 * @example if ( !(txt = change_space(
 * &ret, &text, BUFSIZ, 0 )) ) {...}
**/
int change_space( struct space *space, size_t want, int dir );
#define need_space( space, want ) change_space( space, want, 0 )
#define more_space( space, want ) change_space( space, want, 1 )
#define less_space( space, want ) change_space( space, want, -1 )
#define free_space( space ) (void)change_space( space, 0, 0 )
	
typedef unsigned int node_t;
typedef struct nodes {
	node_t focus;
	node_t count;
	node_t total;
	space_t space;
} nodes_t, text_t, scan_t;

/** @brief Node allocater, passes over to change_space() after doing
 * calculations
 * @param nodes Desitination of allocated memory, initialise with {0}
 * @param Nsize size of each node, sizeof(T) is best practice here
 * @param want Number of nodes you want
 * @param dir Direction to restraint allocation to, see below defines
 * @return 0 on success or errno.h code
**/
int change_nodes(
	struct nodes *nodes, size_t Nsize, node_t want, int dir
);
#define need_nodes( T, nodes, want ) \
	change_nodes( nodes, sizeof(T), want, 0 )
#define more_nodes( T, nodes, want ) \
	change_nodes( nodes, sizeof(T), want, 1 )
#define less_nodes( T, nodes, want ) \
	change_nodes( nodes, sizeof(T), want, -1 )
#define free_nodes( nodes ) (void)change_nodes( nodes, 0, 0, 0 )
/** @brief Adds another node to the end of the node array
 * @param nodes Source node array to select from
 * @param node index of node selected
 * @param Nsize passed to more_nodes if allocation deemed needed
 * @return EXIT_SUCCESS or error number
 * @note Will allocate (nodes->count + 100) nodes if
 * nodes->count >= nodes->total
**/
int add_node( nodes_t *nodes, node_t *node, size_t Nsize );

typedef struct kvpair {
	space_t full, key, val;
} kvpair_t;

typedef struct option {
	char *opt, *key, *val;
	space_t space;
} option_t;

int append_to_option(
	option_t *option, char const *set, char const *txt
);

/** @brief Passes to change_space() and error checks
 * @param kvpair Desitination of memory that is allocated
 * @param want4full Bytes to give kvpair->full
 * @param want4key Bytes to give kvpair->key
 * @param want4val Bytes to give kvpair->val
 * @param dir Direction of allocation
 * @return EXIT_SUCCESS or error returned by change_space
**/
int change_kvpair(
	kvpair_t *kvpair,
	size_t want4full, size_t want4key, size_t want4val, int dir );

/** @brief Processes all arguments after argv[0] common to the gasp
 * executables
 * @param argc pass directly from main() function
 * @param argv pass directly from main() function
 * @param ARGS Needed to pass back allocated memory, should last
 * lifetime of the process
 * @param _leng Needed to pass back the total number of characters
 * needed to print as a command (minus argv[0])
 * @return EXIT_SUCCESS or an error from errno.h
**/
int arguments( int argc, char *argv[], nodes_t *ARGS, size_t *_leng );

void gasp_close_pipes( int pipes[2] );
int dump_files__dir( space_t *space, long inst, bool make );
int gasp_open( int *fd, char const *path, int perm );
int gasp_isdir( char const *path, bool make );
#define gasp_mkdir( path ) gasp_isdir( path, 1 )
int gasp_rmdir( space_t *space, char const *path, bool recursive );

/* We use concept of glance and notice here because it is possible
 * for a process to have died by the time we try to open it, similar
 * to the snapshot concept in win32 but this is tidier :) */
 
typedef struct proc_thread {
	int tid;
	bool active, paused;
	pthread_t thread;
	void *pointer;
} proc_thread_t;

#define SIZEOF_PROCESS_BASE_PATH bitsof(int)

typedef struct process {
	bool self, hooked, paused, exited;
	int pid, parent;
	/* Used to internally mimic behaviour if not natively supported */
	mode_t flags;
	proc_thread_t thread; 
	text_t name, cmdl;
	kvpair_t kvpair;
#ifdef _WIN32
	HANDLE handle;
#else
	char path[SIZEOF_PROCESS_BASE_PATH];
#endif
} process_t;

typedef struct pglance {
	int underId;
	nodes_t nodes;
	process_t process;
} pglance_t;

typedef struct mapped {
	mode_t perm, prot;
	uintmax_t head, foot, size;
} mapped_t;

#define DUMP_LOC_NODE_USED	0
#define DUMP_LOC_NODE_DATA	1
#define DUMP_LOC_NODE_UPTO	2
#define DUMP_LOC_NODE_HALF	0x2000
#define DUMP_LOC_NODE_SIZE (DUMP_LOC_NODE_HALF * 2)
#define DUMP_LOC_NODE(I) (DUMP_LOC_NODE_##I * DUMP_LOC_NODE_SIZE)
#define DUMP_LOC_FULL_SIZE DUMP_LOC_NODE(UPTO)

typedef struct dump_file {
	int fd;
	gasp_off_t prv;
	gasp_off_t pos;
	size_t size;
	uchar *data;
} dump_file_t;

typedef struct dump {
	/* Scan number */
	node_t number;
	
	/* How many bytes are kept from previous part of region if
	 * in same region or foot of last region matches head of current
	 * region */
	size_t kept;
	dump_file_t info, used, data;
	/* Mappings */
	nodes_t *nodes;
	/* How much of nmap has been read */
	uintmax_t done;
	/* Which address to report */
	uintmax_t addr;
	/* Size of data read */
	size_t size;
	/* Where to start comparing from */
	size_t base;
	/* Where to stop comparing upto */
	size_t last;
	/* DUMP_LOC_NODE_HALF + size */
	size_t tail;
	/* Various buffers */
	uchar _[DUMP_LOC_FULL_SIZE], *__[DUMP_LOC_NODE_UPTO];
} dump_t;

/** @brief Wipes all to 0 then sets pointers and file descriptors
 * @return 0 on success, errno.h code on failure
**/
int dump_files_init( dump_t *dump );
int dump_files_open( dump_t *dump, long inst, long done );
int dump_files_test( dump_t dump );
void dump_files_shut( dump_t *dump );
int dump_files_reset_offsets( dump_t *dump, bool read_info );

typedef struct tscan {
	bool wants2rdwr;
	bool ready2wait;
	bool wants2free;
	bool writeable;
	bool dumping;
	bool scanning;
	bool threadmade;
	bool pipesmade;
	bool assignedID;
	int main_pipes[2], scan_pipes[2], zero;
	pthread_t thread;
	int ret;
	node_t id;
	dump_t dump[2];
	nodes_t mappings;
	
	nodes_t	locations;
	
	process_t *process;
	node_t done_scans, regions, found, last_found;
	char compare;
	void * array;
	uintmax_t bytes;
	uintmax_t from;
	uintmax_t upto;
	uintmax_t done_upto;
} tscan_t;

void *proc_handle_dumper( void *_tscan );

/** @brief Thread to scan for bytes in
 * @param _tscan expects to hold a tscan_t object
 **/
void* bytescanner( void *_tscan );


void process_init( process_t *process );

#define PROCESS_O_EX	01
#define PROCESS_O_WR	02
#define PROCESS_O_WX	03
#define PROCESS_O_RD	04
#define PROCESS_O_RX	05
#define PROCESS_O_RW	06
#define PROCESS_O_RWX	07

/** @brief Load information for noticed process
 * @param process Where to place noticed process information
 * @param pid ID of the process to load information of
 * @param hook If should open a permanent handle to the process
 * @param flags Currently unsupported, ignore until this text changes
 * @note Although we can load information in notice it is possible for
 * a process to die before we try to open it's memory thus we keep that
 * attempt to the proc_handle_open() function, even thereafter it is
 * possible for the captured process to die in while the handle is open
 * but as long as it is open then failures will not be critical but
 * rather an indication that the handle is no longer useful
 * @return 0 on success, errno.h code on failure
**/
int process_info( process_t *process, int pid, bool hook, int flags );
/** @brief Cleans up allocations and data
 * @param notice The entry to clear up
 * @note mainly for internal usage
**/
void process_term( process_t *process );
int process_next( pglance_t *glance );
/** @brief Iterates through each entry in /proc
 * @param glance Object to store allocations and data
 * @param underId ID of ancestor, < 0 here will produce 0 results
 * @return 0 on success, errno.h code on failure
**/
int pglance_init( pglance_t *glance, int underId );
/** @brief Cleans up any left over allocations and data
 * @param glance The object to cleanup
 * @note Will ignore NULL pointers
**/
void pglance_term( pglance_t *glance );
/** @brief Scans a glance for processes that contain the given name
 * @param name The name to look for
 * @param nodes Where to place all the noticed processes
 * @param underId ID of ancestor all must have
 * @param usecase If true then is case sensitive search
 * @return 0 on success, errno.h code on failure
**/
int process_find(
	char const *name, nodes_t *nodes, int underId, bool usecase
);
/** @brief Get size of given file path
 * @param path Path of file to get size of
 * @param size Variable to fill with file size
 * @return 0 on success, errno.h code on failure
 * @note certain files will NOT give a size, when this happens as a
 * work around this will read BUFSIZ bytes at a time until EOF is
 * reached or read bytes < BUFSIZ and the cumulative size will be given
**/
int file_glance_size( char const *path, ssize_t *size );
/** @brief Get permissions of given file path
 * @param path Path of file to get size of
 * @param perm Variable to fill with st_mode permissions
 * @return 0 on success, errno.h code on failure
**/
int file_glance_perm( char const *path, mode_t *perm );
/** @brief Set permissions of given file path
 * @param path Path of file to get size of
 * @param perm st_mode Permissions to override with
 * @return 0 on success, errno.h code on failure
**/
int file_change_perm( char const *path, mode_t perm );
bool gasp_tpolli( int int_pipes[2], int *sig, int msTimeout );
bool gasp_tpollo( int ext_pipes[2], int sig, int msTimeout );
/** @brief used to load the next dumpped region into memory, appends
 * to existing first then if not a boundary match (meaning
 * prev foot not equal to next head)
 * @return 0 on success, code from errno.h on failure
**/
int dump_files_glance_stored( dump_t *dump, size_t keep );
/** @brief Deallocates any memory before deallocating the handle itself
 * @param handle The process handle to deallocate
**/
node_t proc_handle_dump( tscan_t *tscan );
/** @brief Reads what was in memory at the time of the read/pread call
 * @param err Where to pass errors to
 * @param handle Handle opened with proc_handle_open()
 * @param addr Where to read from
 * @param dst Where to read into
 * @param size Number of bytes to read into dst
 * @return Number of bytes read into dst
**/
int pglance_data(
	process_t *process,
	uintmax_t addr, void *dst, ssize_t size, ssize_t *done );
/** @brief Writes what was in memory at the time of the write/pwrite call
 * @param err Where to pass errors to
 * @param handle Handle opened with proc_handle_open()
 * @param addr Where to write from
 * @param src Array of bytes to write out
 * @param size Number of bytes to write from src
 * @return Number of bytes written from src
**/
int proc_change_data(
	process_t *process,
	uintmax_t addr, void *src, ssize_t size, ssize_t *done );

extern bool g_reboot_gui;
void lua_create_gasp(lua_State *L);
void lua_create_proc_classes( lua_State *L );
void* lua_extract_bytes(
	int *err, lua_State *L, int index, nodes_t *dst );
int lua_process_find( lua_State *L );
int lua_pglance_grab( lua_State *L );
int lua_process_grab( lua_State *L );
int lua_panic_cb( lua_State *L );
int lua_proc_load_glance( lua_State *L );
int lua_proc_free_glance( lua_State *L );
void lua_error_cb( lua_State *L, char const *text );

void push_global_cfunc(
	lua_State *L, char const *key, lua_CFunction val
);
void push_global_str( lua_State *L, char const *key, char *val );
void push_global_int( lua_State *L, char const *key, int val );
void push_global_num( lua_State *L, char const *key, long double val );
void push_global_bool( lua_State *L, char const *key, bool val );
void push_global_obj( lua_State *L, char const *key );
void push_branch_cfunc(
	lua_State *L, char const *key, lua_CFunction val
);
void push_branch_str( lua_State *L, char const *key, char *val );
void push_branch_int( lua_State *L, char const *key, int val );
void push_branch_num( lua_State *L, char const *key, long double val );
void push_branch_bool( lua_State *L, char const *key, bool val );
void push_branch_obj( lua_State *L, char const *key );
bool find_branch_key( lua_State *L, char const *key );
bool find_branch_str( lua_State *L, char const *key, char const **val );
bool find_branch_int( lua_State *L, char const *key, int *val );
bool find_branch_num( lua_State *L, char const *key, long double *val );
bool find_branch_bool( lua_State *L, char const *key, bool *val );

#endif
