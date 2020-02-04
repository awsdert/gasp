USE_MINGW:=
CC=clang

HAS_DOS:=$(if $(COMSPEC),1,)

IS_WINDOWS:=$(if $(ProgramFiles),1,)
IS_MSDOS:=$(if $(IS_WINDOWS),,$(HAS_DOS))
IS_WIN64:=$(if ${ProgramFiles(x86)},1,)
IS_WIN32:=$(if $(IS_WIN64),,$(IS_WINDOWS))
IS_MACOS:=
IS_LINUX:=$(if $(IS_WINDOWS),,1)

exe_ext:=$(if $(IS_WINDOWS),exe,$(if $(IS_MACOS),app,elf))
dll_ext:=$(if $(IS_WINDOWS),dll,so)

mkdir=$(info $(if $(wildcard $1),,$(shell mkdir $1)))
clone=$(info $(if $(wildcard $1),,$(shell cd $2 && git clone https://github.com/$3)))
pull=$(info $(shell git -C '$1' pull))

#gasp stands for Gaming Assistive tech for Solo Play
#Wanted a more fun name than just plain cheat and since
#there are people who need to cheat to be able to play
#(for example the elderly or physically challanged)
#I thought to tie in the name to such audience, let's
#see gaming companies find an excuse for exlcuding that
#audience when they eventually find a way to block this :)
gasp_sources=space.c nodes.c proc.c arguments.c
gasp_objects=$(gasp_sources:%=%.o)
gasp_dbgobjs=$(gasp_sources:%=%-d.o)
exposed_gasp_objects:=gasp.c.o $(gasp_objects)
exposed_gasp_dbgobjs:=gasp.c-d.o $(gasp_dbgobjs)
debugger_gasp_dbgobjs:=debugger_gasp.c-d.o $(gasp_dbgobjs)
private_gasp_objects:=private_gasp.c.o $(gasp_objects)
private_gasp_dbgobjs:=private_gasp.c-d.o $(gasp_dbgobjs)
gasp_exe:=gasp.$(exe_ext)
gasp_dbg_exe:=gasp-d.$(exe_ext)
debugger_gasp_exe:=debugger_$(gasp_dbg_exe)
private_gasp_exe:=private_$(gasp_exe)
private_gasp_dbg_exe:=private_$(gasp_dbg_exe)

.PHONY: default all run gede gasp debug makefile clean
default: run

lua_dir=cloned/lua
moongl_dir=cloned/moongl
moonglfw_dir=cloned/moonglfw
moonnuklear_dir=cloned/moonnuklear
lua_src_dir:=$(lua_dir)
moongl_src_dir:=$(moongl_dir)/src
moonglfw_src_dir:=$(moonglfw_dir)/src
moonnuklear_src_dir:=$(moonnuklear_dir)/src

$(call mkdir,cloned)
$(call clone,$(lua_dir),cloned,lua/lua)
$(call clone,$(moongl_dir),cloned,stetre/moongl)
$(call clone,$(moonglfw_dir),cloned,stetre/moonglfw)
$(call clone,$(moonnuklear_dir),cloned,stetre/moonnuklear)
$(call pull,$(lua_dir))
$(call pull,$(moongl_dir))
$(call pull,$(moonglfw_dir))
$(call pull,$(moonnuklear_dir))

gasp_args=-D HELLO="WORLD"
bin_flags=-fPIC
rpath=-L'.' -Wl,-rpath,'.'
exe_flags=$(bin_flags) -ldl -lm -lpthread -llua
lib_flags=$(exe_flags) -shared
inc_flags=-I $(lua_dir) -I $(moonnuklear_dir) -I $(moonglfw_dir) -I $(moongl_dir)
win32_defines=_WIN32
linux_defines=_GNU_SOURCE LINUX LUA_USE_LINUX LUA_USE_READLINE
defines:=$(if $(IS_LINUX),$(linux_defines),$(win32_defines))
src_flags=$(bin_flags) -Wall -std=c99 $(defines:%=-D %) -DLUAVER=5.3
dbg_flags=-ggdb -D _DEBUG

libs: lua_lib moon_libs

lua_lib:
	cd $(lua_dir) && make
	cp $(lua_src_dir)liblua.so liblua.so

moon_libs:
	cd $(moongl_dir) && make
	cd $(moonglfw_dir) && make
	cd $(moonnuklear_dir) && make
	cp $(moongl_src_dir)/moongl.so moongl.so
	cp $(moonglfw_src_dir)/moonglfw.so moonglfw.so
	cp $(moonnuklear_src_dir)/moonnuklear.so moonnuklear.so

clean:
	rm -f *.elf
	rm -f *.o
	rm -f $(lua_src_dir)/*.o
	rm -f $(moongl_src_dir)/*.o
	rm -f $(moonglfw_src_dir)/*.o
	rm -f $(moonnuklear_src_dir)/*.o
	rm -f *.so
	rm -f $(lua_src_dir)/*.so
	rm -f $(moongl_src_dir)/*.so
	rm -f $(moonglfw_src_dir)/*.so
	rm -f $(moonnuklear_src_dir)/*.so

leaks: gasp debug
	valgrind -s ./$(private_gasp_dbg_exe) $(gasp_args)
	valgrind -s ./$(debugger_gasp_exe) $(gasp_args)
	valgrind -s ./$(gasp_dbg_exe) $(gasp_args)
	valgrind -s ./$(private_gasp_exe) $(gasp_args)
	valgrind -s ./$(gasp_exe) $(gasp_args)

run: gasp
	./$(gasp_exe) $(gasp_args)

gede: debug
	gede --args ./$(gasp_dbg_exe) $(gasp_args)

gasp: $(gasp_exe) $(private_gasp_exe)

debug: $(gasp_dbg_exe) $(private_gasp_dbg_exe) $(debugger_gasp_exe)

$(gasp_exe): $(exposed_gasp_objects)
	$(CC) $(exe_flags) -o $@ $(exposed_gasp_objects)

$(private_gasp_exe): $(private_gasp_objects)
	$(CC) $(exe_flags) -o $@ $(private_gasp_objects)

$(gasp_dbg_exe): $(exposed_gasp_dbgobjs)
	$(CC) $(dbg_flags) $(exe_flags) -o $@ $(exposed_gasp_dbgobjs)

$(private_gasp_dbg_exe): $(private_gasp_dbgobjs)
	$(CC) $(dbg_flags) $(exe_flags) -o $@ $(private_gasp_dbgobjs)

$(debugger_gasp_exe): $(debugger_gasp_dbgobjs)
	$(CC) $(dbg_flags) $(exe_flags) -o $@ $(debugger_gasp_dbgobjs)

%.o: %
	$(CC) $(flags) $(inc_flags) $(src_flags) -o $@ -c $<

%-d.o: %
	$(CC) $(dbg_flags) $(inc_flags) $(src_flags) -o $@ -c $<

%.so.o:
	@echo Why does the rule '$@' exist!?

%.c: gasp.h

gasp.h: gasp_limits.h

gasp_limits.h:

force: ;
