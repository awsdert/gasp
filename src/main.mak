include target.mak

NAME=main
EXE=$(BIN_DIR)/gasp_$(NAME)$(SYS_EXE_SFX)
LIBRARIES+= pthread
LIBS:=$(LIBRARIES:%=-l%)
OBJECTS:=$(NAME).o pipes.o workers.o memory.o
OBJS=$(OBJECTS:%=$(OBJ_DIR)/%)

$(info MAKECMDGOALS=$(MAKECMDGOALS))
$(info common_goals=$(common_goals))

default: build

rebuild: clean build

build: $(EXE) $(LIBS)

run: $(EXE) $(LIBS)
	$(BIN_DIR)/$(EXE)
	
clean:
	rm -f $(EXE)
	rm -f $(OBJS)

$(EXE): $(OBJS)
	$(CC) $(LIBS) -o $@ $^

lib%: $(SYS_DLL_PFX)%.$(SYS_DLL_SFX)
	make -f $<.mak

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) $(LIBS) -o $@ -c $<

.PHONY: $(common_goals)
