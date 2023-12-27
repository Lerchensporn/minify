#!/usr/bin/env sh

assert()
{
	result="$(echo -e "$2" | ./build/native/minify js -)"
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

input="a() \n b() \n "
expected='a();b()'
assert "$expected" "$input"

input=" ( ) =>  Math . sin ( 1 ) ; "
expected='()=>Math.sin(1)'
assert "$expected" "$input"

input="() => {} \n () => {};"
expected='()=>{};()=>{}'
assert "$expected" "$input"

input='a=0\n/**/\nb=0'
expected='a=0
b=0'
assert "$expected" "$input"

input='{function(){};};a=3'
expected='{function(){}}a=3'
assert "$expected" "$input"

input='( abc ) => 1;'
expected='abc=>1'
assert "$expected" "$input"

input='a=()=>{true};a=3'
expected='a=()=>{!0};a=3'
assert "$expected" "$input"

input='/  /; 3 > /  /;a & /  /'
expected='/  /;3>/  /;a&/  /'
assert "$expected" "$input"

input='function a () {}; function b () {}\n if(true) {} ; a=3'
expected='function a(){}function b(){}if(!0){}a=3'
assert "$expected" "$input"

echo 'Passed all tests'
