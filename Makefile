TARGET		=	bin/3oq
SOURCES		=   $(wildcard src/*.c)
OBJ_PATH	?=  obj
CC			=   gcc
export CFLAGS		?=	-O0 -g
ARFLAGS 	=	rcs

LIBS_PATHS	+= lib/freetype-gl
LIBS_PATHS	+= lib/fileutils
LIBS_PATHS	+= lib/shaderutils
LIBS_PATHS	+= lib/cgeom
LIBS_PATHS	+= lib/stringlib

SYSTEM_LIBS += fontconfig
SYSTEM_LIBS += glew
SYSTEM_LIBS += glfw3

###

#MAKEFLAGS=--no-print-directory -e -s

OBJECTS=$(SOURCES:%.c=$(OBJ_PATH)/%.o)
DEPS=$(OBJECTS:.o=.d)
DEPFLAGS=-MMD -MP

SPACE=$() $()
export PKG_CONFIG_PATH := $(subst $(SPACE),:,$(strip $(LIBS_PATHS)))

LIBS=$(notdir $(LIBS_PATHS))

CFLAGS += $(shell pkg-config --cflags $(LIBS) $(SYSTEM_LIBS))
LDLIBS += $(shell pkg-config --libs $(LIBS) $(SYSTEM_LIBS))

.PHONY: all clean cleanall $(LIBS_PATHS)

all: $(LIBS_PATHS) $(TARGET)

run: $(LIBS_PATHS) $(TARGET)
	$(TARGET)

clean: subprojects.clean
	rm -rf $(OBJ_PATH)

cleanall: subprojects.cleanall clean
	rm -rf bin

subprojects.%:
	$(LIBS_PATHS:%=$(MAKE) -C % $* ;)

$(LIBS_PATHS):
	$(MAKE) -C $@

$(TARGET): $(OBJECTS)
	mkdir -p $(dir $(TARGET))
	$(CC) $(LDFLAGS) $(OBJECTS) -o $@ $(LDLIBS)

$(OBJ_PATH)/%.o: %.c
	mkdir -p $(dir $@)
	$(CC) $(DEPFLAGS) $(CFLAGS) -c $< -o $@

-include $(DEPS)