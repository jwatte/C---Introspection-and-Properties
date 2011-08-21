
# add necessary libs for your UNIX here (-lsocket, -lm, -lstdc++, etc)
LIBS := -g -lpthread
# add whatever C++ flags here you want
CFLAGS := -g -I. -MMD

# what applications to build
APPS := introspection simplechat

# rules to build an app output
define app_rule
bld/$(1):	$$(OBJS_$(1))
	g++ -o $$@ $$^ $$(LIBS)
bld/$(1).obj/%.o:	$(1)/%.cpp
	g++ -c -o $$@ $$< $$(CFLAGS)
endef

define srcs_rule
OBJS_$(1) := $$(patsubst $(1)/%.cpp,bld/$(1).obj/%.o,$$(wildcard $(1)/*.cpp))
endef

# foreach app, define its target .o files
$(foreach app,$(APPS),$(eval $(call srcs_rule,$(app))))

# The main makefile target
all:	bld $(patsubst %,bld/%.obj,$(APPS)) $(patsubst %,bld/%,$(APPS))

# allow cleaning up after ourselves
clean:
	rm -rf bld

# each of the apps
$(foreach app,$(APPS),$(eval $(call app_rule,$(app))))

# the "bld" directory that everything's built into
bld:
	mkdir bld

# the subdirectory to hold the .o files for each app
bld/%.obj:
	mkdir $@

.PHONY:	all	clean

-include $(patsubst %.o,%.d,$(foreach app,$(APPS),$(OBJS_$(app))))

