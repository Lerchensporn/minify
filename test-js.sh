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

input='"abc" + \n "def"'
expected='"abcdef"'
assert "$expected" "$input"

input='/*! do not remove */'
expected='/*! do not remove */'
assert "$expected" "$input"

input='for (let i = 1; ; ++i);'
expected='for(let i=1;;++i);'
assert "$expected" "$input"

input='while(1);'
expected='while(1);'
assert "$expected" "$input"

input='for(1);'
expected='for(1);'
assert "$expected" "$input"

input='if(1);'
expected='if(1);'
assert "$expected" "$input"

input='let obj = {true: true, false: false, undefined: undefined}'
expected='let obj={true:!0,false:!1,undefined:void 0}'
assert "$expected" "$input"

input='+ +i;i+ +1'
expected='+ +i;i+ +1'
assert "$expected" "$input"

input='let a=function(){};let b=3'
expected='let a=function(){};let b=3'
assert "$expected" "$input"

input='(arg) => {} ; () => {} ;  do(() => {})'
expected='arg=>{};()=>{};do(()=>{})'
assert "$expected" "$input"

input='for(;; i++){};/**/;;'
expected='for(;;i++);'
assert "$expected" "$input"

input='if ( 1 ) { /**/ } else  if (1);  else  return ; '
expected='if(1);else if(1);else return'
assert "$expected" "$input"

input='function\n(\n)\n{\n}\n'
expected='function(){}'
assert "$expected" "$input"

input='a()\nb() \n '
expected='a();b()'
assert "$expected" "$input"

input='\n (\n )\n =>\n  Math\n .\n sin\n (\n 1\n )\n ;\n '
expected='()=>Math.sin(1)'
assert "$expected" "$input"

input='() => {} \n () => {};'
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
expected='function a(){}function b(){}if(!0);a=3'
assert "$expected" "$input"

echo 'Passed all tests'
