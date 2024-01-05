#!/usr/bin/env bash

_download()
{
	mkdir test-js-libs
	cd test-js-libs
	wget https://unpkg.com/react@17.0.2/cjs/react.development.js
	wget https://unpkg.com/typescript@5.2.2/lib/typescript.js
}

_main()
{
	for file in test-js-libs/*.js; do
		echo $file
		build/native/minify js $file > minified.js
		node -c minified.js
	done
}

#_download
_main
