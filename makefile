USE_MINGW:=
CC=cc

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
gasp_sources+= lua_common.c lua_process.c
gasp_objs=$(gasp_sources:%=%.o)
gasp_d_objs=$(gasp_sources:%=%-d.o)
init_gasp_objs:=gasp.c.o $(gasp_objs)
init_gasp_d_objs:=gasp.c-d.o $(gasp_d_objs)
test_gasp_d_objs:=test_gasp.c-d.o $(gasp_d_objs)
deep_gasp_objs:=deep_gasp.c.o $(gasp_objs)
deep_gasp_d_objs:=deep_gasp.c-d.o $(gasp_d_objs)
gasp_exe:=gasp.$(exe_ext)
gasp_d_exe:=gasp-d.$(exe_ext)
test_gasp_exe:=test-$(gasp_d_exe)
deep_gasp_exe:=deep-$(gasp_exe)
deep_gasp_d_exe:=deep-$(gasp_d_exe)
PHONY_TARGETS:=makefile default all rebuild clean libs build debug
PHONY_TARGETS+= run gede test

.PHONY: $(PHONY_TARGETS)
default: run

lua_ver=5.3
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

win32_defines=_WIN32
linux_defines=_GNU_SOURCE LINUX LUA_USE_LINUX LUA_USE_READLINE
sys_defines:=$(if $(IS_LINUX),$(linux_defines),$(win32_defines))

src__warning:=-Wall -Wextra -Wpedantic
src__commmon:=-std=c99
src__defines:=$(sys_defines:%=-D %) -D LUAVER=$(lua_ver)
src_combined:=$(src__warning) $(src__commmon) $(src__defines)

ARGS=
rpath=-L"$1" -Wl,-rpath,"$1"
libraries:=lua dl m pthread
LIBS:=$(libraries:%=-l%)
LFLAGS=-L "$(lua_src_dir)"
IFLAGS=-I $(if $(IS_WINDOWS),$(lua_dir),/usr/include/lua$(lua_ver))
BFLAGS:=$(if $(IS_WINDOWS),-fPIC,-fpic)
CFLAGS=$(BFLAGS) $(IFLAGS) $(src_combined)
DFLAGS=-ggdb -D _DEBUG
RPATH:=$(call rpath,.)

all: libs build debug
rebuild: clean libs build

libs: moon_libs

#lua_lib:
#	cd $(lua_dir) && make
#	cp $(lua_src_dir)/liblua.so liblua.so

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

leaks: build debug
	valgrind -s ./$(deep_gasp_d_exe) $(ARGS)
	valgrind -s ./$(test_gasp_exe) $(ARGS)
	valgrind -s ./$(gasp_d_exe) $(ARGS)
	valgrind -s ./$(deep_gasp_exe) $(ARGS)
	valgrind -s ./$(gasp_exe) $(ARGS)

run: build
	./$(gasp_exe) $(ARGS)

gede: debug
	gede --args ./$(gasp_d_exe) $(ARGS)

test: $(deep_gasp_d_exe)
	gede --args $(deep_gasp_d_exe)

build: $(gasp_exe) $(deep_gasp_exe)

debug: $(gasp_d_exe) $(deep_gasp_d_exe) $(test_gasp_exe)

$(gasp_exe): $(init_gasp_objs)
	$(CC) $(BFLAGS) -o $@ $(init_gasp_objs) $(LIBS)

$(deep_gasp_exe): $(deep_gasp_objs)
	$(CC) $(BFLAGS) -o $@ $(deep_gasp_objs) $(LIBS)

$(gasp_d_exe): $(init_gasp_d_objs)
	$(CC) $(DFLAGS) $(BFLAGS) -o $@ $(init_gasp_d_objs) $(LIBS)

$(deep_gasp_d_exe): $(deep_gasp_d_objs)
	$(CC) $(DFLAGS) $(BFLAGS) -o $@ $(deep_gasp_d_objs) $(LIBS)

$(test_gasp_exe): $(test_gasp_d_objs)
	$(CC) $(DFLAGS) $(BFLAGS) -o $@ $(test_gasp_d_objs) $(LIBS)

%.o: %
	$(CC) $(CFLAGS) -o $@ -c $< $(LIBS)

%-d.o: %
	$(CC) $(DFLAGS) $(CFLAGS) -o $@ -c $< $(LIBS)

%.so.o:
	@echo Why does the rule '$@' exist!?

%.c: gasp.h

gasp.h: gasp_limits.h

gasp_limits.h:

force: ;
