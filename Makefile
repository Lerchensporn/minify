CROSS_TRIPLE ?= native
build_dir = build/$(CROSS_TRIPLE)

.PHONY: release
release: $(build_dir)/elfmincss

$(build_dir)/elfmincss: elfmincss.c
	mkdir -p $(build_dir)
	cc -O3 -o $(build_dir)/elfmincss elfmincss.c
	strip $(build_dir)/elfmincss

.PHONY: debug
debug: build/debug/elfmincss

build/debug/elfmincss: elfmincss.c
	$(eval build_dir = build/debug)
	mkdir -p $(build_dir)
	cc --debug -o $(build_dir)/elfmincss elfmincss.c

.PHONY: test
test: release
	PATH=$(build_dir) ./test.sh

.PHONY: clean
clean:
	rm -rf build

.PHONY: crossbuild
cross:
	docker run -v $$(pwd):/app -w /app -u $$(id -u):$$(id -g) multiarch/crossbuild sh -c ' \
		CROSS_TRIPLE=x86_64-apple-darwin make; \
		CROSS_TRIPLE=x86_64-w64-mingw32 make; \
	'
