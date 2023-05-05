#!/usr/bin/env sh

assert()
{
	result="$(echo -e "$2" | ./build/native/minify xml -)"
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

input='<xml></xml><xml></xmll>'
expected='<xml/><xml></xmll>'
assert "$expected" "$input"

input='<xml> <xml> </xml> <xml> </xml> </xml>'
expected='<xml><xml> </xml><xml> </xml></xml>'
assert "$expected" "$input"

input=' <xml a = " b " > <b>  </b><a /><a b="c"/> </xml>'
expected='<xml a=" b "><b>  </b><a/><a b="c"/></xml>'
assert "$expected" "$input"

echo 'Passed all tests'
