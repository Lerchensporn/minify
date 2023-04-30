#!/usr/bin/env bash

for i in {0..1000}; do
	input=$(tr -dc 'A-Za-z0-9!"#$%&'\''()*+,-./:;<=>?@[\]^_`{|}~' </dev/urandom | head -c 1000)
	elfmincss <(echo -n -e "$input") > /dev/null
	if [ $? -gt 1 ]; then
		echo -n $input > input-causing-crash.txt
		echo Crash to reproduce: elfmincss input-causing-crash.txt
		exit
	fi
done
