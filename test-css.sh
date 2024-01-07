#!/usr/bin/env sh

assert()
{
	result="$(echo -e "$2" | ./build/native/minify css -)"
	if [ "$?" != "0" ]; then
		echo 'Crashed on:'
		echo "$2"
		exit 1
	elif [ "$1" != "$result" ]; then
		echo 'Error: expected:'
		echo "$1"
		echo 'got:'
		echo "$result"
		exit 1
	fi
}

input='/*! do not remove */'
expected='/*! do not remove */'
assert "$expected" "$input"

input='a /**/ + /*o/*o*/ a  #b { c : 1px/**//**/1px;/**/; ;;d:"/* "  3 ;; }'
expected='a+a #b{c:1px 1px;d:"/* " 3}'
assert "$expected" "$input"

input='@import url(/*o);'
expected='@import url(/*o);'
assert "$expected" "$input"

input='@import url(  "o"  );@import url(  o  );'
expected='@import url("o");@import url(o);'
assert "$expected" "$input"

input='a{b:c !important}'
expected='a{b:c!important}'
assert "$expected" "$input"

input='@media {a :hover{a :hover}}'
expected='@media{a :hover{a:hover}}'
assert "$expected" "$input"

input='@media all and (hover:hover){} @media (hover:hover){}'
expected='@media all and (hover:hover){}@media(hover:hover){}'
assert "$expected" "$input"

input='a [ padding = 0.1em 1em ] {}'
expected='a[padding=.1em 1em]{}'
assert "$expected" "$input"

input='@media ( width < 0.1em ) {}'
expected='@media(width<.1em){}'
assert "$expected" "$input"

input='@page :left { }'
expected='@page :left{}'
assert "$expected" "$input"

input='a\{b{}'
expected='a\{b{}'
assert "$expected" "$input"

echo 'Passed all tests'
