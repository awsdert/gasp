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

.PHONY: default all gasp debug makefile clean
default: gasp

lua_dir=cloned/lua
moongl_dir=cloned/moongl
moonglfw_dir=cloned/moonglfw
moonnuklear_dir=cloned/moonnuklear

$(call mkdir,cloned)
$(call clone,$(lua_dir),cloned,lua/lua)
$(call clone,$(moongl_dir),cloned,stetre/moongl)
$(call clone,$(moonglfw_dir),cloned,stetre/moonglfw)
$(call clone,$(moonnuklear_dir),cloned,stetre/moonnuklear)
$(call pull,$(lua_dir))
$(call pull,$(moongl_dir))
$(call pull,$(moonglfw_dir))
$(call pull,$(moonnuklear_dir))

lua_src_dir=$(lua_dir)
lua_sources:=$(filter-out $(lua_src_dir)/lua.c $(lua_src_dir)/onelua.c,$(wildcard $(lua_src_dir)/*.c))
lua_objects:=$(lua_sources:%=%.o)
lua_dbgobjs:=$(lua_sources:%=%-d.o)
lua_dll:=liblua.$(dll_ext)
lua_dbg:=liblua-d.$(dll_ext)

moongl_src_dir:=$(moongl_dir)/src
moongl_sources:=$(wildcard $(moongl_src_dir)/*.c)
moongl_objects:=$(moongl_sources:%=%.o)
moongl_dbgobjs:=$(moongl_sources:%=%-d.o)
moongl_dll:=libmoongl.$(dll_ext)
moongl_dbg:=libmoongl-d.$(dll_ext)
moonglfw_src_dir:=$(moonglfw_dir)/src
moonglfw_sources:=$(wildcard $(moonglfw_src_dir)/*.c)
moonglfw_objects:=$(moonglfw_sources:%=%.o)
moonglfw_dbgobjs:=$(moonglfw_sources:%=%-d.o)
moonglfw_dll:=libmoonglfw.$(dll_ext)
moonglfw_dbg:=libmoonglfw-d.$(dll_ext)
moonnuklear_src_dir:=$(moonnuklear_dir)/src
moonnuklear_sources:=$(wildcard $(moonnuklear_src_dir)/*.c)
moonnuklear_objects:=$(moonnuklear_sources:%=%.o)
moonnuklear_dbgobjs:=$(moonnuklear_sources:%=%-d.o)
moonnuklear_dll:=libmoonnuklear.$(dll_ext)
moonnuklear_dbg:=libmoonnuklear-d.$(dll_ext)
bin_flags=-fPIC
rpath=-L'.' -Wl,-rpath,'.'
exe_flags=$(bin_flags) -ldl -lm
lib_flags=$(exe_flags) -shared
inc_flags=-I $(lua_dir) -I $(moonnuklear_dir) -I $(moonglfw_dir) -I $(moongl_dir)
win32_defines=_WIN32
linux_defines=_GNU_SOURCE LINUX LUA_USE_LINUX LUA_USE_READLINE
defines:=$(if $(IS_LINUX),$(linux_defines),$(win32_defines))
src_flags=$(bin_flags) -Wall -std=c99 $(defines:%=-D %) -DLUAVER=5.3
dbg_flags=-ggdb -D _DEBUG

moon_libs_original:
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

leaks: $(gasp_dbg_exe)
	valgrind --leak-check=full --show-leak-kinds=all ./$(gasp_dbg_exe)

gasp: $(gasp_exe) $(private_gasp_exe)
	./$(gasp_exe) -D HELLO="WORLD"

$(gasp_exe): $(exposed_gasp_objects) $(moonnuklear_dll)
	$(CC) $(exe_flags) -llua -o $@ $(exposed_gasp_objects)

$(private_gasp_exe): $(private_gasp_objects) $(moonnuklear_dll)
	$(CC) $(exe_flags) -llua -o $@ $(private_gasp_objects)

debug: $(gasp_dbg_exe) $(private_gasp_dbg_exe) $(debugger_gasp_exe)
	gede --args ./$(gasp_dbg_exe) -D HELLO="WORLD"

$(gasp_dbg_exe): $(exposed_gasp_dbgobjs) $(moonnuklear_dbg)
	$(CC) $(dbg_flags) $(exe_flags) -llua -o $@ $(exposed_gasp_dbgobjs)

$(private_gasp_dbg_exe): $(private_gasp_dbgobjs) $(moonnuklear_dbg)
	$(CC) $(dbg_flags) $(exe_flags) -llua -o $@ $(private_gasp_dbgobjs)

$(debugger_gasp_exe): $(debugger_gasp_dbgobjs) $(moonnuklear_dbg)
	$(CC) $(dbg_flags) $(exe_flags) -llua -o $@ $(debugger_gasp_dbgobjs)

$(moonnuklear_dll): $(moonnuklear_objects) $(moonglfw_dll)
	$(CC) $(lib_flags) -llua -o $@ $(moonnuklear_objects)

$(moonnuklear_dbg): $(moonnuklear_dbgobjs) $(moonglfw_dbg)
	$(CC) $(dbg_flags) $(lib_flags) -llua -o $@ $(moonnuklear_dbgobjs)

$(moonglfw_dll): $(moonglfw_objects) $(moongl_dll)
	$(CC) $(lib_flags) -llua -o $@ $(moonglfw_objects)

$(moonglfw_dbg): $(moonglfw_dbgobjs) $(moongl_dbg)
	$(CC) $(dbg_flags) $(lib_flags) -llua -o $@ $(moonglfw_dbgobjs)

$(moongl_dll): $(moongl_objects) $(lua_dll)
	$(CC) $(lib_flags) -llua -o $@ $(moongl_objects)

$(moongl_dbg): $(moongl_dbgobjs) $(lua_dbg)
	$(CC) $(dbg_flags) $(lib_flags) -llua -o $@ $(moongl_dbgobjs)

$(lua_dll): $(lua_objects)
	$(CC) $(lib_flags) -o $@ $^

$(lua_dbg): $(lua_dbgobjs)
	$(CC) $(dbg_flags) $(lib_flags) -o $@ $^

$(moongl_src_dir)/%.o: $(wildcard $(moongl_src_dir)/*.h)
$(moonglfw_src_dir)/%.o: $(wildcard $(moonglfw_src_dir)/*.h)
$(moonnuklear_src_dir)/%.o: $(wildcard $(moonnuklear_src_dir)/*.h)

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
