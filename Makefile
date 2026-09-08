.PHONY: test format format-check

CPP_FILES := $(shell find Source Tests/src -type f \( -name '*.h' -o -name '*.cpp' \) ! -path '*/vendor/*')

test:
	$(MAKE) -C Tests test

format:
	clang-format -i $(CPP_FILES)

format-check:
	clang-format --dry-run --Werror $(CPP_FILES)
