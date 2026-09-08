CXX      ?= clang++
CXXFLAGS ?= -std=c++17 -O0 -g -Wall -Wextra -Wpedantic -Wno-missing-field-initializers
INCLUDES  = -Ivendor -I../Source/DynamicTrainLogistics/Public

SRC       = src/main.cpp src/quantity_test.cpp src/domain_smoke_test.cpp
BIN       = build/dtl_tests

$(BIN): $(SRC) | build
	$(CXX) $(CXXFLAGS) $(INCLUDES) $(SRC) -o $@

build:
	@mkdir -p build

.PHONY: test clean
test: $(BIN)
	./$(BIN)

clean:
	rm -rf build
