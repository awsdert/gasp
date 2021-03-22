include target.mak

SHARED_NAMES=moonnuklear moonglfw moongl
SHARED=$(SHARED_NAMES:%=lib%.so)
SHARED_LIBS:=$(SHARED_LIBS:%=%$(SYS_DLL_SFX))

build: update $(LIB_DIR)

clone: $(CLONED) $(SHARED_NAMES:%=$(CLONED)/%)

$(LIB_DIR) $(CLONED):
	$(mkdir $@)

$(CLONED)/%: %
	cd "$(CLONED)" && git clone https://github.com/stetre/$<

update: $(SHARED_LIBS)

lib%.so: %
	cd "$(CLONED)/$<" && git pull
	cd "$(CLONED)/$<" && make
	cp "$(CLONED)/$</src/$@" "$(LIB_DIR)/$@"
	

.PHONY: $(common_goals) $(SHARED_LIBS) _copy
