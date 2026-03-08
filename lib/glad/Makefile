TARGET			=	libglad.a
SOURCES			=   src/glad.c
OBJ_PATH		?=  obj
CC				=   gcc
CFLAGS			?=	-O3
INCLUDE_DIRS 	=   include
ARFLAGS 		=	rcs

###

CFLAGS += $(addprefix -I, $(INCLUDE_DIRS))
OBJECTS=$(SOURCES:%.c=$(OBJ_PATH)/%.o)
DEPS=$(OBJECTS:.o=.d)
DEPFLAGS=-MMD -MP

.PHONY: all clean cleanall

all: $(TARGET)

clean:
	rm -rf $(OBJ_PATH)

cleanall: clean
	rm -rf $(TARGET)

$(TARGET): $(OBJECTS)
	$(AR) $(ARFLAGS) $@ $(OBJECTS)

$(OBJ_PATH)/%.o: %.c Makefile
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

-include $(DEPS)