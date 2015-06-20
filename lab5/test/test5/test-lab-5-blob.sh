#!/bin/bash
TEST_FILE=$1
SRCFILE=$2

dd if=${SRCFILE} of=${TEST_FILE} bs=1K seek=3 count=10 >/dev/null 2>&1


