MAKE?=make
MAKECMDGOALS?=default
$(MAKECMDGOALS):
	cd src/ && $(MAKE) $(MAKEFILES) $(MAKECMDGOALS)

.PHONY: $(MAKECMDGOALS)
