CXX      ?= clang++
CXXFLAGS ?= -std=c++17 -O0 -g -Wall -Wextra -Wpedantic -Wno-missing-field-initializers
INCLUDES  = -Ivendor -I../Source/DynamicTrainLogistics/Public

DOMAIN_SRC = $(wildcard ../Source/DynamicTrainLogistics/Private/Domain/*.cpp)
TEST_SRC   = $(wildcard src/*.cpp)
SRC        = $(TEST_SRC) $(DOMAIN_SRC)
BIN        = build/dtl_tests

HEADERS = $(wildcard ../Source/DynamicTrainLogistics/Public/Domain/*.h) $(wildcard vendor/*.hpp)

$(BIN): $(SRC) $(HEADERS) Makefile | build
	$(CXX) $(CXXFLAGS) $(INCLUDES) $(SRC) -o $@

build:
	@mkdir -p build

.PHONY: test clean
test: $(BIN)
	./$(BIN)

clean:
	rm -rf build
