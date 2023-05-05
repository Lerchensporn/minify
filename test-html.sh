#!/usr/bin/env sh

assert()
{
	result="$(echo -e "$2" | ./build/native/minify html -)"
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

input='<html prop="abc"/>'
expected='<html prop=abc>'
assert "$expected" "$input"

input='<html></html>'
expected='<html></html>'
assert "$expected" "$input"

input='<html> <html> </html> <html> </html> </html>'
expected='<html> <html></html> <html></html> </html>'
assert "$expected" "$input"

input='<html>  <!---->'
expected='<html>'
assert "$expected" "$input"

echo 'Passed all tests'
