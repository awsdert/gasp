MAKECMDGOALS?=main default
common_goals=default run clean
COMMONGOALS:=$(filter $(common_goals),$(MAKECMDGOALS))
TARGETGOALS:=$(filter-out $(common_goals),$(MAKECMDGOALS))

# This makes separating goals much easier as whatever variables are set will
# not interfere with the variables of another goal, also makes it clear to
# developers when they've made a typo in the goal... on linux at least, not so
# much on windows

$(TARGETGOALS):
	make -f $@.mak $(COMMONGOALS)
