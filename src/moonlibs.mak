include target.mak

SHARED_NAMES=moonnuklear moonglfw moongl
SHARED_LIBS:=$(SHARED_LIBS:%=%$(SYS_DLL_SFX))

moongl_DEV=stetre
moonglfw_DEV=stetre
moonnuklear_DEV=stetre

is_dir=$(if $(wildcard $1 $1/*),$2,$3)
mkdir=$(call is_dir,$1,echo "$1" already exists!,mkdir "$1")
git_pull=cd $1$(call is_dir,$1/$2,/$2 && git pull, && git clone $3)
github_pull=$(call git_pull,$(CLONED),$1,https://github.com/$2.git)

pull: $(SHARED_NAMES)

rebuild: clean build

build: $(SHARED_NAMES)

clean:
	rm -f $(LIB_DIR)/*.so
	rm -f $(CLONED)/moongl/src/*.so $(CLONED)/moongl/src/*.o
	rm -f $(CLONED)/moonglfw/src/*.so $(CLONED)/moonglfw/src/*.o
	rm -f $(CLONED)/moonnuklear/src/*.so $(CLONED)/moonnuklear/src/*.o

$(SHARED_NAMES):
	@$(call mkdir,$(CLONED))
	@$(call mkdir,$(LIB_DIR))
	$(call github_pull,$<,$($<_DEV)/$<)
	cd $(CLONED)/$@ && make
	cp $(CLONED)/$@/src/$@$(SYS_DLL_SFX) $(LIB_DIR)/$@$(SYS_DLL_SFX)

.PHONY: $(common_goals) $(SHARED_NAMES) $(SHARED_LIBS) _copy

.FORCE:
