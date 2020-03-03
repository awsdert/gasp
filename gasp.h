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
 * @param err Thread safe passback of encountered errors, in the event
 * malloc or realloc fail then this will be filled with whatever errno
 * holds at that time
 * @param space Desitination of allocated memory, initialise with {0}
 * @param want Amount of memory you want
 * @param dir Direction to restraint allocation to, see below defines
 * @return allocated memory or NULL, nothing in space parameter will be
 * overwritten if memory failes to be retrieved, if freed will be
 * cleared
 * @example if ( !(txt = change_space(
 * &ret, &text, BUFSIZ, 0 )) ) {...}
**/
void* change_space( int *err, space_t *space, size_t want, int dir );
#define need_space( err, space, want )\
	change_space( err, space, want, 0 )
#define more_space( err, space, want )\
	change_space( err, space, want, 1 )
#define less_space( err, space, want )\
	change_space( err, space, want, -1 )
#define free_space( err, space )\
	change_space( err, space, 0, 0 )
	
int gasp_isdir( char const *path );
int gasp_mkdir( space_t *space, char const *path );
int gasp_rmdir( space_t *space, char const *path, bool recursive );
	
typedef size_t node_t;
typedef struct nodes {
	node_t count;
	node_t total;
	space_t space;
} nodes_t, scan_t;

/** @brief Node allocater, passes over to change_space() after doing
 * calculations
 * @param err Thread safe passback of encountered errors, in the event
 * malloc or realloc fail then this will be filled with whatever errno
 * holds at that time
 * @param nodes Desitination of allocated memory, initialise with {0}
 * @param Nsize size of each node, sizeof(T) is best practice here
 * @param want Number of nodes you want
 * @param dir Direction to restraint allocation to, see below defines
 * @return allocated memory or NULL, nothing in nodes parameter will be
 * overwritten if memory failes to be retrieved, if freed will be
 * cleared
 * @example if ( !(txt = change_space(
 * &ret, &text, sizeof(wchar_t), BUFSIZ, 0 )) ) {...}
**/
void* change_nodes(
	int *err, nodes_t *nodes, size_t Nsize, node_t want, int dir );
#define need_nodes( T, err, nodes, want )\
	(T*)change_nodes( err, nodes, sizeof(T), want, 0 )
#define more_nodes( T, err, nodes, want )\
	(T*)change_nodes( err, nodes, sizeof(T), want, 1 )
#define less_nodes( T, err, nodes, want )\
	(T*)change_nodes( err, nodes, sizeof(T), want, -1 )
#define free_nodes( T, err, nodes )\
	(T*)change_nodes( err, nodes, sizeof(T), 0, 0 )
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

/* We use concept of glance and notice here because it is possible
 * for a process to have died by the time we try to open it, similar
 * to the snapshot concept in win32 but this is tidier :) */
typedef struct proc_notice {
	bool self;
	int entryId, ownerId;
	space_t name;
	space_t cmdl;
	kvpair_t kvpair;
} proc_notice_t;
typedef struct proc_glance {
	int underId;
	node_t process;
	nodes_t idNodes;
	proc_notice_t notice;
} proc_glance_t;
typedef struct proc_mapped {
	mode_t perm, prot;
	uintmax_t head, foot, size;
} proc_mapped_t;
#define SIZEOF_PROCDIR (sizeof(int) * CHAR_BIT)
typedef struct proc_handle {
	bool running, waiting, attached, exited;
	pthread_t thread;
	proc_notice_t notice;
#ifdef _WIN32
	HANDLE handle;
#else
	/* Should only hold /proc/%d or /proc/self */
	char procdir[SIZEOF_PROCDIR];
#ifdef gasp_pread
	int memfd, perm;
#endif
#endif
} proc_handle_t;

#define DUMP_BUF_SIZE BUFSIZ
#define DUMP_MAX_SIZE (DUMP_BUF_SIZE * 2)
typedef struct dump {
	int info_fd;
	int used_fd;
	int data_fd;
	/* Iterate through memory by swapping pointers to the below */
	proc_mapped_t mapped[2], *pmap, *nmap;
	/* How much of nmap has been read */
	uintmax_t done;
	/* Which address to report */
	uintmax_t addr;
	/* Where to stop comparing from 0 */
	size_t size;
	uchar used[DUMP_MAX_SIZE], data[DUMP_MAX_SIZE];
} dump_t;

int open_dump_files( dump_t *dump, long inst, long done );
int check_dump( dump_t dump );
void shut_dump_files( dump_t *dump );

typedef struct tscan {
	bool wants2rdwr;
	bool ready2wait;
	bool wants2free;
	bool writeable;
	bool dumping;
	bool scanning;
	bool threadmade;
	bool pipesmade;
	int main_pipe[2], scan_pipe[2], zero;
	pthread_t thread;
	int ret;
	dump_t dump[2];
	
	proc_handle_t *handle;
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

/** @brief free's allocated memory and zeroes everything
 * @param notice The entry to clear up
 * @note mainly for internal usage
**/
void proc_notice_zero( proc_notice_t *notice );
proc_notice_t* proc_notice_next( int *err, proc_glance_t *glance );
/** @brief Load information for noticed process
 * @param err Where to pass error codes back to, always of errno.h codes
 * @param pid ID of the process to load information of
 * @param notice Where to place noticed process information
 * @note Although we can load information in notice it is possible for
 * a process to die before we try to open it's memory thus we keep that
 * attempt to the proc_handle_open() function, even thereafter it is
 * possible for the captured process to die in while the handle is open
 * but as long as it is open then failures will not be critical but
 * rather an indication that the handle is no longer useful
**/
proc_notice_t* proc_notice_info(
	int *err, int pid, proc_notice_t *notice );
/** @brief Iterates through each entry in /proc
 * @param err Where to pass back errors to
 * @param glance Where to store directory handle and related info
 * @param underId ID of ancestor, invalid IDs will not trigger errors
 * but will produce 0 results
**/
proc_notice_t* proc_glance_init(
	int *err, proc_glance_t *glance, int underId );
/** @brief Deallocates any memory, closes handles and zeroes everything
 * @param glance The glance to cleanup
**/
void proc_glance_term(
	proc_glance_t *glance );
/** @brief Searches through a glance for processes that contain
 * the given name
 * @param err Where to pass error codes to
 * @param name The name to look for
 * @param nodes Where to place all the noticed processes
 * @param underId ID of ancestor all must have
 * @param usecase If true then is case sensitive search
 * @return nodes->space.block
**/
proc_notice_t* proc_locate_name(
	int *err, char const *name, nodes_t *nodes, int underId,
	bool usecase );
/** @brief opens a file, seeks to end takes the offset then closes it
 * @param err Where to pass errors back to
 * @param path Path of file to get size of
 * @return Size of file or 0
**/
size_t file_glance_size(
	int *err, char const *path );
/** @brief Get permissions of given path
 * @param err Where to pass errors back to
 * @param path Path of file to get permissions of
 * @return st_mode or 0
**/
mode_t file_glance_perm(
	int *err, char const *path );
/** @brief Set permissions of given path
 * @param err Where to pass errors back to
 * @param path Path of file to get permissions of
 * @param perm The permissions you want
 * @return Original st_mode
**/
mode_t file_change_perm(
	int *err, char const *path, mode_t perm );
/** @brief Opens file descriptors and anything deemed neccessary for
 * handling the process, also calls ptrace with PTRACE_SEIZE or
 * PTRACE_ATTACH if that is unavailable
 * @param err Where to pass errors back to
 * @param pid ID of process to open handle for
 * @return Handle of process or NULL
**/
proc_handle_t* proc_handle_open( int *err, int pid );
/** @brief used to load the next dumpped region into memory, appends
 * to existing first then if not a boundary match (meaning
 * prev foot not equal to next head)
**/
bool glance_dump( int *err, dump_t *dump, bool *both, size_t keep );
/** @brief Deallocates any memory before deallocating the handle itself
 * @param handle The process handle to deallocate
**/
void proc_handle_shut(
	proc_handle_t* handle );
node_t proc_handle_dump( tscan_t *tscan );
/** @brief Scans process mapped pages for array between from and upto
 * @param err Where to pass errors to
 * @param prev_fd File descriptor of file to read address list into
 * @param next_fd File descriptor of file to write address list into
 * @param handle Handle opened with proc_handle_open()
 * @param array Array of bytes to look for
 * @param bytes Number of bytes in array
 * @param from Where to start search
 * @param upto Where to end search
 * @param writable Must be writable if this is true
 * @return Count of addresses that matched/where written to file
**/
node_t proc_aobscan(
	int *err, int prev_fd, int next_fd, scan_t *scan,
	proc_handle_t *handle,
	uchar *array, uintmax_t bytes,
	uintmax_t from, uintmax_t upto,
	bool writable );
/** @brief Scans process mapped pages for array between from and upto
 * @param err Where to pass errors to
 * @param init_fd File descriptor of file to write address list into
 * @param handle Handle opened with proc_handle_open()
 * @param array Array of bytes to look for
 * @param bytes Number of bytes in array
 * @param from Where to start search
 * @param upto Where to end search
 * @param writable Must be writable if this is true
 * @return Count of addresses that matched/where written to file
**/
node_t proc_aobinit(
	int *err, int init_fd, scan_t *scan,
	proc_handle_t *handle,
	uchar *array, uintmax_t bytes,
	uintmax_t from, uintmax_t upto,
	bool writable );
/** @brief Reads what was in memory at the time of the read/pread call
 * @param err Where to pass errors to
 * @param handle Handle opened with proc_handle_open()
 * @param addr Where to read from
 * @param dst Where to read into
 * @param size Number of bytes to read into dst
 * @return Number of bytes read into dst
**/
uintmax_t proc_glance_data(
	int *err, proc_handle_t *handle,
	uintmax_t addr, void *dst, size_t size );
/** @brief Writes what was in memory at the time of the write/pwrite call
 * @param err Where to pass errors to
 * @param handle Handle opened with proc_handle_open()
 * @param addr Where to write from
 * @param src Array of bytes to write out
 * @param size Number of bytes to write from src
 * @return Number of bytes written from src
**/
uintmax_t proc_change_data(
	int *err, proc_handle_t *handle,
	uintmax_t addr, void *src, size_t size );

extern bool g_reboot_gui;
void lua_create_gasp(lua_State *L);
void lua_create_proc_classes( lua_State *L );
void* lua_extract_bytes(
	int *err, lua_State *L, int index, nodes_t *dst );
int lua_proc_locate_name( lua_State *L );
int lua_proc_glance_grab( lua_State *L );
int lua_proc_handle_grab( lua_State *L );
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
