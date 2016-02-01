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

YUGONG_CXX_SRCS = $(wildcard src/*.cpp)
YUGONG_CXX_HDRS = $(wildcard src/*.hpp)
YUGONG_CXX_OUTS = $(foreach F,$(YUGONG_CXX_SRCS),$(patsubst src/%.cpp,target/%.o,$(F)))
YUGONG_ASM_SRCS = $(wildcard src/*.S)
YUGONG_ASM_HDRS = $(wildcard src/*.S.inc)
YUGONG_ASM_OUTS = $(foreach F,$(YUGONG_ASM_SRCS),$(patsubst src/%.S,target/%.o,$(F)))

YUGONG_SRCS = $(YUGONG_CXX_SRCS) $(YUGONG_ASM_SRCS)
YUGONG_HDRS = $(YUGONG_CXX_HDRS) $(YUGONG_ASM_HDRS)
YUGONG_OUTS = $(YUGONG_CXX_OUTS) $(YUGONG_ASM_OUTS)

YUGONG_AR = target/libyugong.a

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

$(YUGONG_CXX_OUTS): target/%.o: src/%.cpp $(YUGONG_HDRS)
	$(CXX) -c $(CXXFLAGS) $< -o $@

$(YUGONG_ASM_OUTS): target/%.o: src/%.S $(YUGONG_ASM_HDRS)
	$(CXX) -c $(CXXFLAGS) $< -o $@

$(YUGONG_AR): $(YUGONG_OUTS)
	$(AR) cr $(YUGONG_AR) $(YUGONG_OUTS)

.PHONY: tests

tests: $(TEST_OUTS)

$(TEST_OUTS): target/%: tests/%.cpp $(YUGONG_AR)
	$(CXX) $(TEST_CXXFLAGS) $< $(YUGONG_AR) -o $@

.PHONY: clean

clean:
	rm -r target
