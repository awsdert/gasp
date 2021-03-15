include target.mak

NAME=main
EXE=gasp_$(NAME)$(SYS_EXE_SFX)
LIBS:=

default: $(EXE) $(LIBS)

run: $(EXE) $(LIBS)
	$(BIN_DIR)/$(EXE)

$(EXE):
	$(CC) $(CFLAGS) $(SRC_DIR)/$(NAME).c -o $@

lib%: $(SYS_DLL_PFX)%.$(SYS_DLL_SFX)
	make -f $<.mak
