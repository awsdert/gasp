include common_goals.mak
include platform.mak
include compiler.mak

LIBRARIES:=
DEFINES:=$(CPU_DEFINES) $(SYS_DEFINES) $(CC_DEFINES)

ifneq '$(filter debug gede,$(MAKECMDGOALS))' ''
	DFLAGS+= -ggdb
	DEFINES+= _DEBUG
	DBG_SFX:=_d
endif

BIN_DIR?=../bin
LIB_DIR?=../OTG/lib
OBJ_DIR?=../obj
INC_DIR?=../include
SRC_DIR?=.
CLONED?=../cloned

$(info BIN_DIR=$(BIN_DIR))
$(info LIB_DIR=$(LIB_DIR))
$(info OBJ_DIR=$(OBJ_DIR))
$(info INC_DIR=$(INC_DIR))
$(info SRC_DIR=$(SRC_DIR))
$(info CLONED=$(CLONED))

CFLAGS+=$(DFLAGS) -I "$(INC_DIR)" $(DEFINES:%=-D %)
