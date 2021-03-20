include target.mak

SHARED_NAMES=moonnuklear moonglfw moongl
SHARED=$(SHARED_NAMES:%=lib%.so)

build: update $(SHARED_NAMES:%=lib%.so)

clone: $(CLONED) $(SHARED_NAMES:%=$(CLONED)/%)

$(CLONED):
	$(mkdir $@)

$(CLONED)/%: %
	$(mkdir $@)
	cd "$(CLONED)" && git clone https://github.com/stetre/$<

update: $(SHARED_NAMES)
	cd "$(CLONED)/$<" && git pull

$(SHARED_NAMES):

lib%.so: %
	cd "$(CLONED)/$<" && make
	cp "$(CLONED)/$</lib/$@" "$(LIB_DIR)/$@"

.PHONY: clone update $(SHARED_NAMES:%=%_clone)
