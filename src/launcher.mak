include target.mak

NAME=launcher
EXE=gasp$(SYS_EXE_SFX)

default: $(EXE)

run: $(EXE)
	$(BIN_DIR)/$(EXE)

$(EXE):
	$(CC) $(CCFLAGS) $(NAME).c -o $@
