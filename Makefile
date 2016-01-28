ifndef OS
    uname := $(shell uname)
    ifeq ($(uname),Darwin)
	OS = OSX
    else
    ifeq ($(uname),Linux)
	OS = LINUX
    else
	$(error Unrecognized operating system $(uname))
    endif
    endif
endif

CFLAGS += -std=gnu11
CXXFLAGS += -std=gnu++11

YUGONG_CXX_SRCS = src/yugong.cpp
YUGONG_CXX_HDRS = src/yugong.hpp
YUGONG_ASM_SRCS = src/yg_asm.S
YUGONG_ASM_HDRS = src/yg_asm_common.S.inc
YUGONG_SRCS = $(YUGONG_CXX_SRCS) $(YUGONG_ASM_SRCS)
YUGONG_HDRS = $(YUGONG_CXX_HDRS) $(YUGONG_ASM_HDRS)

YUGONG_TARGETS = target/yugong.o target/yg_asm.o

TEST_SRCS = $(wildcard tests/test_*.cpp)
TEST_OUTS = $(foreach F,$(TEST_SRCS),$(patsubst tests/%.cpp,target/%,$(F)))
TEST_CXXFLAGS = $(CXXFLAGS) -I src

.SECONDEXPANSION:

.PHONY: all

all: dir src tests

.PHONY: dir

dir: target

target:
	mkdir -p target

.PHONY: src

src: $(YUGONG_TARGETS)

target/yugong.o: src/yugong.cpp $(YUGONG_CXX_HDRS)
	$(CXX) -c $(CXXFLAGS) $< -o $@

target/yg_asm.o: src/yg_asm.S $(YUGONG_ASM_HDRS)
	$(CXX) -c $(CXXFLAGS) $< -o $@

.PHONY: tests

tests: $(TEST_OUTS)

$(TEST_OUTS): target/%: tests/%.cpp $(YUGONG_TARGETS)
	$(CXX) $(TEST_CXXFLAGS) $< $(YUGONG_TARGETS) -o $@

.PHONY: clean

clean:
	rm -r target
