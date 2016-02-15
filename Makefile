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

LLVM_CONFIG := $(shell ./detect-llvm.sh)
LLVM_INCLUDEDIR := `$(LLVM_CONFIG) --includedir`
LLVM_CXXFLAGS := `$(LLVM_CONFIG) --cxxflags`
LLVM_LDFLAGS := `$(LLVM_CONFIG) --ldflags --libs --system-libs`

C_CXX_FLAGS_COMMON = -Wall -I deps/libunwind/include
override CFLAGS += -std=gnu11 $(C_CXX_FLAGS_COMMON)
override CXXFLAGS += -std=gnu++11 $(C_CXX_FLAGS_COMMON)

ifeq ($(OS),LINUX)
    #override LDFLAGS += -lunwind -lunwind-x86_64
endif

YUGONG_DIR = yugong
YUGONG_CXX_SRCS = $(wildcard $(YUGONG_DIR)/*.cpp)
YUGONG_CXX_HDRS = $(wildcard $(YUGONG_DIR)/*.hpp)
YUGONG_CXX_OUTS = $(foreach F,$(YUGONG_CXX_SRCS),$(patsubst $(YUGONG_DIR)/%.cpp,target/%.o,$(F)))
YUGONG_ASM_SRCS = $(wildcard $(YUGONG_DIR)/*.S)
YUGONG_ASM_HDRS = $(wildcard $(YUGONG_DIR)/*.S.inc)
YUGONG_ASM_OUTS = $(foreach F,$(YUGONG_ASM_SRCS),$(patsubst $(YUGONG_DIR)/%.S,target/%.o,$(F)))

YUGONG_SRCS = $(YUGONG_CXX_SRCS) $(YUGONG_ASM_SRCS)
YUGONG_HDRS = $(YUGONG_CXX_HDRS) $(YUGONG_ASM_HDRS)
YUGONG_OUTS = $(YUGONG_CXX_OUTS) $(YUGONG_ASM_OUTS)

YUGONG_AR = target/libyugong.a

YUGONG_LLVM_DIR = yugong-llvm
YUGONG_LLVM_SRCS = $(wildcard $(YUGONG_LLVM_DIR)/*.cpp)
YUGONG_LLVM_HDRS = $(wildcard $(YUGONG_LLVM_DIR)/*.hpp)
YUGONG_LLVM_OUTS = $(foreach F,$(YUGONG_LLVM_SRCS),$(patsubst $(YUGONG_LLVM_DIR)/%.cpp,target/%.o,$(F)))
YUGONG_LLVM_CXXFLAGS = $(CXXFLAGS) -I $(YUGONG_DIR) $(LLVM_CXXFLAGS) -pthread

YUGONG_LLVM_AR = target/libyugong-llvm.a

TESTS_DIR = tests
TESTS_SRCS = $(wildcard $(TESTS_DIR)/test_*.cpp)
TESTS_HDRS = $(wildcard $(TESTS_DIR)/test_*.hpp)
TESTS_OUTS = $(foreach F,$(TESTS_SRCS),$(patsubst $(TESTS_DIR)/%.cpp,target/%,$(F)))
TESTS_CXXFLAGS = $(CXXFLAGS) -I $(YUGONG_DIR)

TESTS_LLVM_SRCS = $(wildcard $(TESTS_DIR)/test-llvm_*.cpp)
TESTS_LLVM_HDRS = $(wildcard $(TESTS_DIR)/test-llvm_*.hpp)
TESTS_LLVM_OUTS = $(foreach F,$(TESTS_LLVM_SRCS),$(patsubst $(TESTS_DIR)/%.cpp,target/%,$(F)))
TESTS_LLVM_CXXFLAGS = $(CXXFLAGS) $(LLVM_CXXFLAGS) -I $(YUGONG_DIR) -I $(YUGONG_LLVM_DIR)

LIBUNWIND_AR = target/libunwind.a

.SECONDEXPANSION:

.PHONY: all dir libunwind src yugong yugong-llvm

all: dir libunwind src tests

dir: target

target:
	mkdir -p target

libunwind: $(LIBUNWIND_AR)

$(LIBUNWIND_AR):
	mkdir -p target/libunwind-build
	(cd target/libunwind-build; cmake ../../deps/libunwind -DLLVM_CONFIG=`which $(LLVM_CONFIG)` && make)
	$(AR) cr $(LIBUNWIND_AR) target/libunwind-build/src/CMakeFiles/unwind.dir/*.o

src: yugong yugong-llvm

yugong: $(YUGONG_AR)

yugong-llvm: $(YUGONG_LLVM_AR)

$(YUGONG_AR): $(YUGONG_OUTS)
	$(AR) cr $(YUGONG_AR) $(YUGONG_OUTS)

$(YUGONG_CXX_OUTS): target/%.o: $(YUGONG_DIR)/%.cpp $(YUGONG_HDRS)
	$(CXX) -c $(CXXFLAGS) $< -o $@

$(YUGONG_ASM_OUTS): target/%.o: $(YUGONG_DIR)/%.S $(YUGONG_ASM_HDRS)
	$(CXX) -c $(CXXFLAGS) $< -o $@

$(YUGONG_LLVM_AR): $(YUGONG_LLVM_OUTS)
	$(AR) cr $(YUGONG_LLVM_AR) $(YUGONG_LLVM_OUTS)

$(YUGONG_LLVM_OUTS): target/%.o: $(YUGONG_LLVM_DIR)/%.cpp $(YUGONG_HDRS) $(YUGONG_LLVM_HDRS)
	$(CXX) -c $(YUGONG_LLVM_CXXFLAGS) $< -o $@

.PHONY: tests

tests: $(TESTS_OUTS) $(TESTS_LLVM_OUTS)

$(TESTS_OUTS): target/%: $(TESTS_DIR)/%.cpp $(TESTS_HDRS) $(YUGONG_AR) $(LIBUNWIND_AR)
	$(CXX) $(TESTS_CXXFLAGS) $< $(YUGONG_AR) $(LIBUNWIND_AR) $(LDFLAGS) -o $@

$(TESTS_LLVM_OUTS): target/%: $(TESTS_DIR)/%.cpp $(TESTS_HDRS) $(TESTS_LLVM_HDRS) $(YUGONG_AR) $(YUGONG_LLVM_AR) $(LIBUNWIND_AR)
	$(CXX) $(TESTS_LLVM_CXXFLAGS) $< $(YUGONG_AR) $(YUGONG_LLVM_AR) $(LIBUNWIND_AR) $(LDFLAGS) $(LLVM_LDFLAGS) -o $@

.PHONY: clean

clean:
	rm -r target

.PHONY: showincludedirs

showincludedirs:
	@echo $(realpath $(YUGONG_DIR))
	@echo $(realpath $(YUGONG_LLVM_DIR))
	@echo $(LLVM_INCLUDEDIR)

