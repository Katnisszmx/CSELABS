#!/usr/bin/env bash

ulimit -c unlimited

LOSSY=$1
NUM_LS=$1

if [ -z $NUM_LS ]; then
    NUM_LS=0
fi

BASE_PORT1=$RANDOM
BASE_PORT1=$[BASE_PORT1+2000]
EXTENT_PORT1=$BASE_PORT1
YFS1_PORT=$[BASE_PORT1+2]
LOCK_PORT=$[BASE_PORT1+6]

YFSDIR1=$PWD/yfs1

#=========================== preparation ============================
echo -n Preparing
if [ "$LOSSY" ]; then
    export RPC_LOSSY=$LOSSY
fi

if [ $NUM_LS -gt 1 ]; then
    x=0
    rm config
    while [ $x -lt $NUM_LS ]; do
      port=$[LOCK_PORT+2*x]
      x=$[x+1]
      echo $port >> config
    done
    x=0
    while [ $x -lt $NUM_LS ]; do
      port=$[LOCK_PORT+2*x]
      x=$[x+1]
#      echo "starting ./lock_server $LOCK_PORT $port > lock_server$x.log 2>&1 &"
      ./lock_server $LOCK_PORT $port > lock_server$x.log 2>&1 &
      sleep 1
    done
else
#    echo "starting ./lock_server $LOCK_PORT > lock_server.log 2>&1 &"
    ./lock_server $LOCK_PORT > lock_server.log 2>&1 &
    sleep 1
fi

echo -n ...

unset RPC_LOSSY

#echo "starting ./extent_server $EXTENT_PORT1 > extent_server1.log 2>&1 &"
./extent_server $EXTENT_PORT1 > extent_server1.log 2>&1 &
sleep 1

echo -n ...

# first start
rm -rf $YFSDIR1
mkdir $YFSDIR1 || exit 1
sleep 1

echo -n ...

#echo "starting ./yfs_client $YFSDIR1 $EXTENT_PORT1 $LOCK_PORT > yfs_client1.log 2>&1 &"
#./yfs_client $YFSDIR1 $EXTENT_PORT1 $LOCK_PORT > yfs_client1.log 2>&1 &
./run-client.sh $YFSDIR1 $EXTENT_PORT1 $LOCK_PORT
sleep 1 

echo Done

#make sure FUSE is mounted where we expect
pwd=`pwd -P`
if [ `mount | grep "$pwd/yfs1" | grep -v grep | wc -l` -ne 1 ]; then
    sh stop.sh
    echo "Failed to mount YFS properly at ./yfs1"
    exit -1
fi
#=====================================================================

TOTAL=90
SCORE=0

SBAD='Lei is a bad TA #-_-'
SGOOD='Lei is a good TA ^u^'

#=========================== Test 1 ======================== ==========

echo -n [1] Crash and restart ..............................
# [Before crash]

# [Crash]
killall yfs_client
sleep 1 

# [Recover]
fusermount -u $YFSDIR1
./run-client.sh $YFSDIR1 $EXTENT_PORT1 $LOCK_PORT
sleep 1

# [After recovery]

# [Check]
if [ `ps | grep yfs_client | wc -l` -eq 1 ]; then
	echo -n '[OK] '
	let SCORE=SCORE+10
else
	echo -n '[FAIL] '
fi
echo [$SCORE/$TOTAL]

#======================================================================

#=========================== Test 2 ======================== ==========

echo -n [2] Simple recovery - read/write/create ............
# [Before crash]
echo $SGOOD > yfs1/file1
echo $SBAD > yfs1/file2

# [Crash]
killall yfs_client
sleep 1 

# [Recover]
fusermount -u $YFSDIR1
./run-client.sh $YFSDIR1 $EXTENT_PORT1 $LOCK_PORT
sleep 1

# [After recovery]

# [Check]
if [[ ( `ps | grep yfs_client | wc -l` -eq 1 ) && ( `cat yfs1/file1` = $SGOOD ) && ( `cat yfs1/file2` = $SBAD ) ]] 
then
	echo -n '[OK] '
	let SCORE=SCORE+15
else
	echo -n '[FAIL] '
fi
echo [$SCORE/$TOTAL]

#======================================================================

#=========================== Test 3 ======================== ==========

echo -n [3] Simple recovery - mkdir/symlink/unlink .........
# [Before crash]
mkdir yfs1/test3/
mkdir yfs1/test3/dir1
echo $SBAD > yfs1/test3/file1
rm yfs1/test3/file1
echo $SBAD > yfs1/test3/dir1/file2
echo $SGOOD > yfs1/test3/file1
echo $SGOOD > yfs1/test3/dir1/file1
tree -a yfs1 > test-lab-5.log

# [Crash]
killall yfs_client
sleep 1 

# [Recover]
fusermount -u $YFSDIR1
./run-client.sh $YFSDIR1 $EXTENT_PORT1 $LOCK_PORT
sleep 1

# [After recovery]

# [Check]
if [[ ( `ps | grep yfs_client | wc -l` -eq 1 ) && ( `cat yfs1/test3/file1` = $SGOOD ) && ( `cat yfs1/test3/dir1/file1` = $SGOOD ) && ( `cat yfs1/test3/dir1/file2` = $SBAD ) ]] 
then
	echo -n '[OK] '
	let SCORE=SCORE+15
else
	echo -n '[FAIL] '
fi
echo [$SCORE/$TOTAL]

#======================================================================

#=========================== Test 4 ======================== ==========

echo -n [4] Crash around concurrent big file writing .......
# [Before crash]
TEST_FILE4_0=test-lab-5-file-comp4
TEST_FILE4_1=yfs1/test4/file1
TEST_FILE4_2=yfs1/test4/file2
TEST_FILE4_3=yfs1/test4/file3
TEST_FILE4_4=yfs1/test4/file4
SRCFILE4=test-lab-5-file-rand
mkdir yfs1/test4
dd if=/dev/urandom of=${SRCFILE4} bs=1K count=400 >/dev/null 2>&1
dd if=${SRCFILE4} of=${TEST_FILE4_0} bs=1K seek=3 count=10 >/dev/null 2>&1
./test-lab-5-blob.sh $TEST_FILE4_2 $SRCFILE4 &
./test-lab-5-blob.sh $TEST_FILE4_1 $SRCFILE4 
./test-lab-5-blob.sh $TEST_FILE4_3 $SRCFILE4 &
./test-lab-5-blob.sh $TEST_FILE4_4 $SRCFILE4 &

# [Crash]
killall yfs_client
sleep 1 

# [Recover]
fusermount -u $YFSDIR1
./run-client.sh $YFSDIR1 $EXTENT_PORT1 $LOCK_PORT
sleep 1

# [After recovery]

# [Check]
if [[ ( `ps | grep yfs_client | wc -l` -eq 1 ) && \
	( -a $TEST_FILE4_1 ) && ( `diff $TEST_FILE4_0 $TEST_FILE4_1 | wc -l ` -eq 0 ) && \
	( !( -a $TEST_FILE4_2 ) || ( `diff $TEST_FILE4_0 $TEST_FILE4_2 | wc -l ` -eq 0 ) ) && \
	( !( -a $TEST_FILE4_4 ) || ( `diff $TEST_FILE4_0 $TEST_FILE4_4 | wc -l ` -eq 0 ) ) && \
	( !( -a $TEST_FILE4_3 ) || ( `diff $TEST_FILE4_0 $TEST_FILE4_3 | wc -l ` -eq 0 ) ) ]]
then
	echo -n '[OK] '
	let SCORE=SCORE+25
else
	echo -n '[FAIL] '
fi
echo [$SCORE/$TOTAL]

# [Clean Up]
rm ${SRCFILE4}

#======================================================================

#=========================== Test 5 ======================== ==========

echo -n '[5] Long running '
# [Before crash]
mkdir yfs1/test5
echo $SGOOD > yfs1/test5/file0
./run-long.sh &
sleep 2
echo -n .......
sleep 2
echo -n .......
sleep 1

# [Crash]
killall yfs_client > /dev/null 2>&1 &
killall test-lab-5-long.sh > /dev/null 2>&1 &
killall echo >/dev/null 2>&1 &
killall rm >/dev/null 2>&1 &
sleep 1
echo -n .......
sleep 1

# [Recover]
fusermount -u $YFSDIR1
./run-client.sh $YFSDIR1 $EXTENT_PORT1 $LOCK_PORT
sleep 1
echo -n .......
sleep 2
echo -n .......

# [After recovery]

# [Check]
if [[ ( `ps | grep yfs_client | wc -l` -eq 1 ) &&
	( `cat yfs1/test5/file0` = $SGOOD ) && \
	( !( -a $TEST_FILE5_1 ) || ( `cat $TEST_FILE5_1` = $SGOOD2 ) ) && \
	( !( -a $TEST_FILE5_2 ) || ( `cat $TEST_FILE5_2` = $SBAD2 ) ) && \
	( !( -a $TEST_FILE5_3 ) || ( `cat $TEST_FILE5_3` = $SGOOD2 ) ) && \
	( !( -a $TEST_FILE5_4 ) || ( `cat $TEST_FILE5_4` = $SBAD2 ) ) ]]
then
	echo -n '[OK] '
	let SCORE=SCORE+25
else
	echo -n '[FAIL] '
fi
echo [$SCORE/$TOTAL]

#======================================================================

if [ $SCORE -eq $TOTAL ]
then
	echo [You\'ve passed all the test! Good Job!]
fi
