COMPILER ?= cc
ifndef CROSS_TRIPLE
	OUTPUT := minify
else ifeq '$(CROSS_TRIPLE)' 'x86_64-w64-mingw32'
	OUTPUT := minify_$(CROSS_TRIPLE).exe
else
	OUTPUT := minify_$(CROSS_TRIPLE)
endif

.PHONY: build
build: build/$(OUTPUT)

build/$(OUTPUT): minify.c
	mkdir -p build
	$(COMPILER) -O2 -Wall -Wno-parentheses -Wno-maybe-uninitialized -o build/$(OUTPUT) minify.c
	strip build/$(OUTPUT)

.PHONY: test
test: release check
	./test-xml.sh
	./test-css.sh
	./test-html.sh
	./test-js.sh
	./test-js-libs.sh

.PHONY: check
check:
	cppcheck --enable=all --suppress=missingIncludeSystem --check-level=exhaustive minify.c

.PHONY: clean
clean:
	rm -rf build

.PHONY: crossbuild
crossbuild:
	docker run -e CROSS_TRIPLE=x86_64-w64-mingw32 \
		-v $$(pwd):/workdir -u $$(id -u):$$(id -g) multiarch/crossbuild make && \
	docker run -e CROSS_TRIPLE=x86_64-apple-darwin \
		-v $$(pwd):/workdir -u $$(id -u):$$(id -g) multiarch/crossbuild make
	docker run -e CROSS_TRIPLE=x86_64-linux-gnu \
		-v $$(pwd):/workdir -u $$(id -u):$$(id -g) multiarch/crossbuild make
