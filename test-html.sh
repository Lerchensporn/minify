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

input='  a  b  '
expected=' a b '
assert "$expected" "$input"

input='</script>; ;'
expected='</script>; ; '
assert "$expected" "$input"

input='<scrIpT TyPe=application/json&plus;ld> { "key" : true } </script>'
expected='<scrIpT TyPe=application/json&plus;ld>{"key":true}</script>'
assert "$expected" "$input"

input='<script type="text&sol;javascript"> { "key" : true } </script>'
expected='<script type=text&sol;javascript>{"key":!0}</script>'
assert "$expected" "$input"

input='<html prop=/>'
expected='<html prop=/>'
assert "$expected" "$input"

input='<html data-yes/>'
expected='<html data-yes>'
assert "$expected" "$input"

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
expected='<html> '
assert "$expected" "$input"

echo 'Passed all tests'
