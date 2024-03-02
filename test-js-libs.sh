#!/usr/bin/env bash

_download()
{
    mkdir -p test-js-libs
    cd test-js-libs
    if [ ! -f react.development.js ]; then
        wget https://unpkg.com/react@17.0.2/cjs/react.development.js
    fi
    if [ ! -f typescript.js ]; then
        wget https://unpkg.com/typescript@5.2.2/lib/typescript.js
    fi
    if [ ! -f vue.js ]; then
        wget https://unpkg.com/vue@2.6.12/dist/vue.js
    fi
    if [ ! -f jquery.js ]; then
        wget https://unpkg.com/jquery@3.5.1/dist/jquery.js
    fi
    if [ ! -f antd.js ]; then
        wget https://unpkg.com/antd@4.16.1/dist/antd.js
    fi
    if [ ! -f echarts.js ]; then
        wget https://unpkg.com/echarts@5.1.1/dist/echarts.js
    fi
    if [ ! -f victory.js ]; then
        wget https://unpkg.com/victory@35.8.4/dist/victory.js
    fi
    if [ ! -f three.js ]; then
        wget https://unpkg.com/three@0.124.0/build/three.js
    fi
    if [ ! -f bundle.min.js ]; then
        wget https://unpkg.com/terser@5.26.0/dist/bundle.min.js
    fi
    if [ ! -f d3.js ]; then
        wget https://unpkg.com/d3@6.3.1/dist/d3.js
    fi
    if [ ! -f lodash.js ]; then
        wget https://unpkg.com/lodash@4.17.21/lodash.js
    fi
    if [ ! -f moment.js ]; then
        wget https://unpkg.com/moment@2.29.1/moment.js
    fi
    cd ..
}

_main()
{
    for file in test-js-libs/*.js; do
        printf '%s:\n   ' "$file"
        build/minify js --benchmark $file || return
        build/minify js $file | node -c || return
    done
}

_download
_main
