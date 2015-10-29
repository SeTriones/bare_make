
TARGET := libcommon.a

DISTRIB_ROOT ?= ..

EXCLUDE_SUBDIRS :=
SUBDIRS := $(shell find . -maxdepth 1 -type d)
SUBDIRS := $(basename $(notdir $(SUBDIRS)))
SUBDIRS := $(filter-out $(EXCLUDE_SUBDIRS),$(SUBDIRS))

DEPS_DIR := .deps

SOURCES := $(wildcard *.cc *.c *.cpp)
OBJS := $(patsubst %.c, %.o, $(SOURCES))
OBJS := $(patsubst %.cc, %.o, $(OBJS))
OBJS := $(patsubst %.cpp, %.o, $(OBJS))
DEPS := $(patsubst %.o, %.d, $(OBJS))
DEPS_FP := $(addprefix $(DEPS_DIR)/, $(DEPS))
DEPS_FP_EXIST := $(wildcard $(DEPS_DIR)/*.d)
DEPS_FP_MISSING := $(filter-out $(DEPS_FP_EXIST), $(DEPS_FP))

RM-RF := rm -rf
CC := gcc
AR := ar rcs
CXX := g++
INCLUDES := -I. -I$(DISTRIB_ROOT)
CFLAGS := -g -O2 -Wall -fPIC
CPPFLAGS := -g -O2 -Wall -fPIC
LFLAGS := -lpthread

.PHONY : all objs dirs clean clean-objs clean-deps subdirs subdirs_clean
all : subdirs dirs $(TARGET)

$(TARGET) : $(OBJS)
	$(AR) $@ $^

objs : dirs $(OBJS)

dirs :
ifeq ($(wildcard $(DEPS_DIR)),)
	@mkdir -p $(DEPS_DIR)
endif

subdirs :
	@for subdir in $(SUBDIRS); \
		do \
			cd $$subdir && make; \
		done

subdirs_clean :
	@for subdir in $(SUBDIRS); \
		do \
			cd $$subdir && make clean; \
		done

.cc.o :
	$(CXX) $(CPPFLAGS) $(INCLUDES) -MMD -MP -MF $(DEPS_DIR)/$(*F).d -c $< -o $@

%.o : %.cpp
	$(CXX) $(CPPFLAGS) $(INCLUDES) -MMD -MP -MF $(DEPS_DIR)/$(*F).d -c $< -o $@

.c.o :
	$(CC) $(CFLAGS) $(INCLUDES) -MMD -MP -MF $(DEPS_DIR)/$(*F).d -c $< -o $@

clean-objs :
	$(RM-RF) $(OBJS)

clean-deps :
	$(RM-RF) $(DEPS_DIR)

clean : subdirs_clean clean-objs clean-deps
	$(RM-RF) $(TARGET)

ifneq ($(DEPS_FP_MISSING),)
$(DEPS_FP_MISSING):
	@$(RM-RF) $(patsubst %.d, %.o, $(@F))
endif

-include $(DEPS_FP)
