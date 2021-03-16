include target.mak

NAME=main
EXE=gasp_$(NAME)$(SYS_EXE_SFX)
LIBRARIES+= pthread
LIBS:=$(LIBRARIES:%=-l %)
OBJECTS:=$(NAME).o pipes.o workers.o
OBJS=$(OBJECTS:%=$(OBJ_DIR)/%)

default: $(EXE) $(LIBS)

run: $(EXE) $(LIBS)
	$(BIN_DIR)/$(EXE)

$(EXE): $(OBJS)
	$(CC) $(LIBS) -o $@ $^

lib%: $(SYS_DLL_PFX)%.$(SYS_DLL_SFX)
	make -f $<.mak

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) $(LIBS) -o $@ -c $<
