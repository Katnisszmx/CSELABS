#!/bin/bash
SGOOD2='LeiIsAGoodTA'
SBAD2='LeiIsABadTA'
TEST_FILE5_1=yfs1/test5/file1
TEST_FILE5_2=yfs1/test5/file2
TEST_FILE5_3=yfs1/test5/file3
TEST_FILE5_4=yfs1/test5/file4
TIMES=1000000
nohup ./test-lab-5-long.sh $TEST_FILE5_1 $TIMES $SGOOD2 >/dev/null 2>&1 &
nohup ./test-lab-5-long.sh $TEST_FILE5_2 $TIMES $SBAD2 >/dev/null 2>&1 &
nohup ./test-lab-5-long.sh $TEST_FILE5_3 $TIMES $SGOOD2 >/dev/null 2>&1 &
