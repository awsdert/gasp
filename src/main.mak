include target.mak

NAME=main
EXE=$(BIN_DIR)/gasp_$(NAME)$(SYS_EXE_SFX)
DBG_EXE=$(EXE)_d
LIBRARIES+= pthread lua
LIBS:=$(LIBRARIES:%=-l%)
DBG_LIBS:=$(LIBRARIES:%=-l%)
OBJECTS:=$(NAME) pipes workers memory
OBJS=$(OBJECTS:%=$(OBJ_DIR)/%.o)
OBJS=$(OBJECTS:%=$(OBJ_DIR)/%_d.o)

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

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) $(LIBS) -o $@ -c $<

$(OBJ_DIR)/%_d.o: $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) $(LIBS) -o $@ -c $<

.PHONY: $(common_goals)
