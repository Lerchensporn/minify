CROSS_TRIPLE ?= native
build_dir = build/$(CROSS_TRIPLE)

.PHONY: build
build: $(build_dir)/minify

$(build_dir)/minify: minify.c
	mkdir -p $(build_dir)
	cc -O2 -o $(build_dir)/minify minify.c
	strip $(build_dir)/minify

.PHONY: debug
debug: build/debug/minify

build/debug/minify: minify.c
	$(eval build_dir = build/debug)
	mkdir -p $(build_dir)
	cc --debug -o $(build_dir)/minify minify.c

.PHONY: test
test: release check
	./test-xml.sh
	./test-css.sh
	./test-html.sh
	./test-js.sh
	./test-js-libs.sh

.PHONY: check
check:
	cppcheck --enable=all --suppress=missingIncludeSystem minify.c

.PHONY: clean
clean:
	rm -rf build

.PHONY: crossbuild
cross:
	docker run -v $$(pwd):/app -w /app -u $$(id -u):$$(id -g) multiarch/crossbuild sh -c ' \
		CROSS_TRIPLE=x86_64-apple-darwin make; \
		CROSS_TRIPLE=x86_64-w64-mingw32 make; \
	'
