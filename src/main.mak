include target.mak

NAME=main
EXE=$(BIN_DIR)/gasp_$(NAME)$(DBG_SFX)$(SYS_EXE_SFX)
LANG?=enGB
LIBRARIES+= pthread lua
LIBS:=$(LIBRARIES:%=-l%)
DBG_LIBS:=$(LIBRARIES:%=-l%)
OBJECTS:=$(NAME) pipes workers memory worker_msg/$(basename $(LANG)) lua_common
OBJS=$(OBJECTS:%=$(OBJ_DIR)/%$(DBG_SFX).o)

$(info MAKECMDGOALS=$(MAKECMDGOALS))
$(info common_goals=$(common_goals))

default: build

rebuild: clean build

run: build
	$(BIN_DIR)/$(EXE)

gede: build
	gede --args ./$(EXE)

build: $(EXE) $(LIBS)
	
clean:
	rm -f $(EXE)
	rm -f $(OBJS)

$(EXE): $(OBJS)
	$(CC) $(DFLAGS) -o $@ $^ $(LIBS)

lib%: $(SYS_DLL_PFX)%.$(SYS_DLL_SFX)
	make -f $<.mak

$(OBJ_DIR)/%$(DBG_SFX).o: $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) $(LIBS) -o $@ -c $<

.PHONY: $(common_goals)
