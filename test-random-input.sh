#!/usr/bin/env bash

export LC_CTYPE=C
for format in css js xml html; do
	for i in {0..1000}; do
		input=$(tr -dc 'A-Za-z0-9!"#$%&'\''()*+,-./:;<=>?@[\]^_`{|}~' </dev/urandom | head -c 1000)
		./build/native/minify $format <(echo -n -e "$input") > /dev/null
		if [ $? -gt 1 ]; then
			echo -n $input > input-causing-crash.txt
			echo Crash to reproduce: minify $format input-causing-crash.txt
			exit
		fi
	done
done

echo Passed the test - no crash occurred
