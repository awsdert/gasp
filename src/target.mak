include common_goals.mak
include platform.mak
include compiler.mak

LIBRARIES:=
DEFINES:=$(CPU_DEFINES) $(SYS_DEFINES) $(CC_DEFINES)

BIN_DIR?=../bin
LIB_DIR?=../lib
OBJ_DIR?=../obj
INC_DIR?=../include
SRC_DIR?=.

CFLAGS+=-I "$(INC_DIR)" $(DEFINES:%=-D %)
