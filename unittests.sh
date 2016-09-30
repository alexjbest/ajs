#!/usr/bin/env bash

TEST_SUBJECT="$1"

if [ -z "$TEST_SUBJECT" ]
then
	echo "Specify a program to test"
	exit 1
fi

if ! echo -e 'mov rax,1+2*(3+4)+5\nret' | "$TEST_SUBJECT" -i -m 1 | grep '	mov	$20,%rax' > /dev/null
then
	exit 1
fi
