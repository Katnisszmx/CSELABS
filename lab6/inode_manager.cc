#include "inode_manager.h"
#include <unistd.h>
#include <fstream>

// disk layer -----------------------------------------

disk::disk()
{
    bzero(blocks, sizeof(blocks));
}

void disk::read_block(blockid_t id, char *buf)
{
    /*
     *your lab1 code goes here.
     *if id is smaller than 0 or larger than BLOCK_NUM
     *or buf is null, just return.
     *put the content of target block into buf.
     *hint: use memcpy
     */
    if(id < 0 || id > BLOCK_NUM || buf == NULL)
        return;
    memcpy(buf, blocks[id], BLOCK_SIZE);
}

void disk::write_block(blockid_t id, const char *buf)
{
    /*
     *your lab1 code goes here.
     *hint: just like read_block
     */
    if(id < 0 || id > BLOCK_NUM || buf == NULL)
        return;
    memcpy(blocks[id], buf, BLOCK_SIZE);
}

// block layer -----------------------------------------

// Allocate a free disk block.
blockid_t block_manager::alloc_block()
{
    /*
     * your lab1 code goes here.
     * note: you should mark the corresponding bit in block bitmap when alloc.
     * you need to think about which block you can start to be allocated.

     *hint: use macro IBLOCK and BBLOCK.
     use bit operation.
     remind yourself of the layout of disk.
     */
    static uint32_t id = IBLOCK(INODE_NUM, BLOCK_NUM);
    char buf[BLOCK_SIZE];
    for(uint32_t i=0; i<BLOCK_NUM; i++){
        d->read_block(BBLOCK(id), buf);
        uint32_t pos = id % BLOCK_SIZE;
        uint32_t* bits = &((uint32_t*)buf)[pos/sizeof(uint32_t)];
        if((*bits & (1 << pos)) == 0){
            *bits |= (1 << pos);
            d->write_block(BBLOCK(id), buf);
            return id++;
        }
        else{
            id++;
            if(id >= BLOCK_NUM){
                id = id%BLOCK_NUM+IBLOCK(INODE_NUM, BLOCK_NUM);
            }
        }
    }
    return -1;
}

void block_manager::free_block(uint32_t id)
{
    /*
     * your lab1 code goes here.
     * note: you should unmark the corresponding bit in the block bitmap when free.
     */
    char buf[BLOCK_SIZE];
    d->read_block(BBLOCK(id), buf);
    uint32_t pos = id % BLOCK_SIZE;
    uint32_t* bits = &((uint32_t*)buf)[pos/sizeof(uint32_t)];
    *bits &= ~(1 << pos);
    d->write_block(BBLOCK(id), buf);
}

// The layout of disk should be like this:
// |<-sb->|<-free block bitmap->|<-inode table->|<-data->|
block_manager::block_manager()
{
    d = new disk();

    // format the disk
    sb.size = BLOCK_SIZE * BLOCK_NUM;
    sb.nblocks = BLOCK_NUM;
    sb.ninodes = INODE_NUM;

}

void block_manager::read_block(uint32_t id, char *buf)
{
    d->read_block(id, buf);
}

void block_manager::write_block(uint32_t id, const char *buf)
{
    d->write_block(id, buf);
}

// inode layer -----------------------------------------

inode_manager::inode_manager()
{
    bm = new block_manager();
    memset(alloc_num, 0, INODE_NUM);
    transnum = 1;
    version = 0;
    logpos = 0;
    logfile.reserve(100);
    pthread_mutex_init(&alclock, 0);
    pthread_mutex_init(&loglock, 0);
    uint32_t root_dir = alloc_inode(extent_protocol::T_DIR);
    if (root_dir != 1) {
        printf("\tim: error! alloc first inode %d, should be 1\n", root_dir);
        exit(0);
    }
    write_file(root_dir, "", 0);
    //pthread_create(&tid, NULL, truncate, (void*)this);
}

inode_manager::~inode_manager(){
    //pthread_cancel(tid);
}

/* Create a new file.
 * Return its inum. */
uint32_t inode_manager::alloc_inode(uint32_t type)
{
    /*
     * your lab1 code goes here.
     * note: the normal inode block should begin from the 2nd inode block.
     * the 1st is used for root_dir, see inode_manager::inode_manager().

     * if you get some heap memory, do not forget to free it.
     */
    static uint32_t inum=1;
    pthread_mutex_lock(&alclock);
    while(alloc_num[inum] != 0){
        pthread_mutex_unlock(&alclock);
        pthread_mutex_lock(&alclock);
        inum = inum % (INODE_NUM - 1) + 1;
    }
    alloc_num[inum] = 1;
    pthread_mutex_unlock(&alclock);

    char buf[BLOCK_SIZE];
    struct inode *ino_disk;
    bm->read_block(IBLOCK(inum, BLOCK_NUM), buf);
    ino_disk = (struct inode*)buf + inum%IPB;
    ino_disk->type = type;
    ino_disk->size = 0;
    ino_disk->atime = ino_disk->mtime = ino_disk->ctime = time(NULL);
    bm->write_block(IBLOCK(inum, BLOCK_NUM), buf);
    return inum;
}

void inode_manager::free_inode(uint32_t inum)
{
    /*
     * your lab1 code goes here.
     * note: you need to check if the inode is already a freed one;
     * if not, clear it, and remember to write back to disk.
     * do not forget to free memory if necessary.
     */
    alloc_num[inum] = 2;
}


/* Return an inode structure by inum, NULL otherwise.
 * Caller should release the memory. */
struct inode* inode_manager::get_inode(uint32_t inum)
{
    struct inode *ino, *ino_disk;
    char buf[BLOCK_SIZE];

    printf("\tim: get_inode %d\n", inum);

    if (inum < 0 || inum >= INODE_NUM) {
        printf("\tim: inum out of range\n");
        return NULL;
    }

    bm->read_block(IBLOCK(inum, bm->sb.nblocks), buf);
    // printf("%s:%d\n", __FILE__, __LINE__);

    ino_disk = (struct inode*)buf + inum%IPB;
    if (alloc_num[inum] != 1) {
        printf("\tim: inode not exist\n");
        return NULL;
    }

    ino = (struct inode*)malloc(sizeof(struct inode));
    *ino = *ino_disk;

    return ino;
}

void inode_manager::put_inode(uint32_t inum, struct inode *ino)
{
    char buf[BLOCK_SIZE];
    struct inode *ino_disk;

    printf("\tim: put_inode %d\n", inum);
    if (ino == NULL)
        return;

    bm->read_block(IBLOCK(inum, bm->sb.nblocks), buf);
    ino_disk = (struct inode*)buf + inum%IPB;
    *ino_disk = *ino;
    bm->write_block(IBLOCK(inum, bm->sb.nblocks), buf);
}

#define MIN(a,b) ((a)<(b) ? (a) : (b))

/* Get all the data of a file by inum.
 * Return alloced data, should be freed by caller. */
void inode_manager::read_file(uint32_t inum, char **buf_out, int *size)
{
    /*
     * your lab1 code goes here.
     * note: read blocks related to inode number inum,
     * and copy them to buf_out
     */
    if(buf_out == NULL || size == NULL)
        return;
    struct inode* ino = get_inode(inum);
    if(ino == NULL)
        return;
    ino->atime = time(NULL);
    put_inode(inum, ino);

    *size = ino->size;
    char* buf = (char*)malloc(*size);
    uint32_t num = (*size + BLOCK_SIZE - 1) / BLOCK_SIZE;
    uint32_t i;
    char tmp[BLOCK_SIZE];
    for(i=0; i<MIN(num, NDIRECT); i++){
        bm->read_block(ino->blocks[i], tmp);
        memcpy(buf+i*BLOCK_SIZE, tmp,
                (i == num-1)? *size-i*BLOCK_SIZE:BLOCK_SIZE);
    }

    if(num > NDIRECT){
        char blk[BLOCK_SIZE];
        bm->read_block(ino->blocks[NDIRECT], blk);
        uint32_t* ip = (uint32_t*)blk;
        for(; i<num; i++){
            bm->read_block(ip[i-NDIRECT], tmp);
            memcpy(buf+i*BLOCK_SIZE, tmp,
                    (i == num-1)? *size-i*BLOCK_SIZE:BLOCK_SIZE);
        }
    }
    *buf_out = buf;
    free(ino);
}

/* alloc/free blocks if needed */
void inode_manager::write_file(uint32_t inum, const char *buf, int size)
{
    /*
     * your lab1 code goes here.
     * note: write buf to blocks of inode inum.
     * you need to consider the situation when the size of buf
     * is larger or smaller than the size of original inode.
     * you should free some blocks if necessary.
     */
    if(size < 0 || (uint32_t)size > BLOCK_SIZE * MAXFILE){
        printf("\tim: error! size negative or too large.\n");
        return;
    }
    struct inode* ino = get_inode(inum);
    if(ino==NULL){
        printf("\tim: error! inode not exist.\n");
        return;
    }

    uint32_t pre_num = (ino->size + BLOCK_SIZE - 1) / BLOCK_SIZE;
    uint32_t num = (size + BLOCK_SIZE - 1) / BLOCK_SIZE;

    uint32_t i, id, *ip;
    char blk[BLOCK_SIZE];
    id = ino->blocks[NDIRECT];
    ip = (uint32_t*)blk;
    if(pre_num < num){
        if(pre_num > NDIRECT){
            bm->read_block(id, blk);
            for(i=pre_num; i<num; i++)
                ip[i-NDIRECT] = bm->alloc_block();
            bm->write_block(id, blk);
        }
        else if(num > NDIRECT){
            for(i=pre_num; i<NDIRECT; i++)
                ino->blocks[i] = bm->alloc_block();
            id = bm->alloc_block();
            ino->blocks[NDIRECT] = id;
            for(i=0; i<num-NDIRECT; i++)
                ip[i] = bm->alloc_block();
            bm->write_block(id, blk);
        }
        else{
            for(i=pre_num; i<num; i++)
                ino->blocks[i] = bm->alloc_block();
        }
    }
    else if(pre_num > num){
        if(num > NDIRECT){
            bm->read_block(id, blk);
            for(i=num; i<pre_num; i++)
                bm->free_block(ip[i-NDIRECT]);
        }
        else if(pre_num > NDIRECT){
            bm->read_block(id, blk);
            for(i=num; i<NDIRECT; i++)
                bm->free_block(ino->blocks[i]);
            for(i=0; i<num-NDIRECT; i++)
                bm->free_block(ip[i]);
        }
        else{
            for(i=num; i<pre_num; i++)
                bm->free_block(ino->blocks[i]);
        }
    }

    char tmp[BLOCK_SIZE];
    for(i=0; i<MIN(num, NDIRECT); i++){
        memcpy(tmp, buf+i*BLOCK_SIZE, BLOCK_SIZE);
        bm->write_block(ino->blocks[i], tmp);
    }

    if(num > NDIRECT){
        bm->read_block(ino->blocks[NDIRECT], blk);
        for(; i<num; i++){
            memcpy(tmp, buf+i*BLOCK_SIZE, BLOCK_SIZE);
            bm->write_block(ip[i-NDIRECT], tmp);
        }
    }

    ino->size = size;
    ino->mtime = time(NULL);
    ino->ctime = time(NULL);
    put_inode(inum, ino);
    free(ino);
}

void inode_manager::getattr(uint32_t inum, extent_protocol::attr &a)
{
    /*
     * your lab1 code goes here.
     * note: get the attributes of inode inum.
     * you can refer to "struct attr" in extent_protocol.h
     */
    struct inode* ino = get_inode(inum);
    if(ino == NULL)
        return;
    a.type = ino->type;
    a.size = ino->size;
    a.atime = ino->atime;
    a.mtime = ino->mtime;
    a.ctime = ino->ctime;
    free(ino);
}

void inode_manager::remove_file(uint32_t inum)
{
    /*
     * your lab1 code goes here
     * note: you need to consider about both the data block and inode of the file
     * do not forget to free memory if necessary.
     */
    struct inode* ino = get_inode(inum);
    if(ino == NULL)
        return;
    uint32_t num = (ino->size + BLOCK_SIZE - 1) / BLOCK_SIZE;
    for(uint32_t i=0; i<MIN(num, NDIRECT); i++)
        bm->free_block(ino->blocks[i]);
    blockid_t id = ino->blocks[NDIRECT];
    if(num>NDIRECT){
        char buf[BLOCK_SIZE];
        bm->read_block(id, buf);
        uint32_t* ip = (uint32_t*)buf;
        for(uint32_t i=0; i<num-NDIRECT; i++)
            bm->free_block(ip[i]);
        bm->free_block(id);
    }
    free(ino);
    free_inode(inum);
}

void inode_manager::log(std::string nlog, uint32_t& num){
    pthread_mutex_lock(&loglock);
    std::stringstream ss;
    if(nlog == "begin"){
        num = transnum++;
        ss << num << ' ' << nlog;
        logfile.push_back(ss.str());
    }
    else{
        ss << nlog;
        uint32_t cmtnum;
        std::string action;
        ss >> cmtnum >> action;
        if(action == "put" || action == "remove"){
            extent_protocol::extentid_t id;
            ss >> id;
            char *buf_out = NULL;
            int size;
            read_file(id, &buf_out, &size);
            std::string buf;
            buf.assign(buf_out, size);
            std::stringstream ts;
            ts << nlog << ' ' << size << ' ' << buf;
            nlog = ts.str();
        }
        else if(action == "end"){
            int i, n=logfile.size();
            for(i=n-1; i>=0; i--){
                std::stringstream ts;
                uint32_t actnum;
                std::string act;
                ts << logfile[i];
                ts >> actnum >> act;
                if(act == "begin" && actnum == cmtnum)
                    break;
            }
            for(i=i+1; i<n; i++){
                std::stringstream ts;
                uint32_t actnum;
                std::string act;
                ts << logfile[i];
                ts >> actnum >> act;
                if(actnum == cmtnum){
                    extent_protocol::extentid_t id;
                    if(act == "put"){
                        uint32_t size;
                        ts >> id >> size;
                        ts.get();
                        char cbuf[size];
                        for(uint32_t j=0; j<size; j++)
                            ts.get(cbuf[j]);
                        write_file(id, cbuf, size);
                    }
                    else if(act == "remove"){
                        ts >> id;
                        remove_file(id);
                    }
                }
            }
        }
        logfile.push_back(nlog);
    }
    pthread_mutex_unlock(&loglock);
}

/*
void inode_manager::recover(std::string& log){
    log = "";
    pthread_mutex_lock(&loglock);
    int n = logfile.size();
    for(int i=0; i<n; i++){
        log = log + logfile[i] + "\n";
    }
    pthread_mutex_unlock(&loglock);
}
*/

void* inode_manager::truncate(void* arg){
    inode_manager* im = (inode_manager*)arg;
    while(true){
        sleep(10);
        pthread_mutex_lock(&im->loglock);
        int n = im->logfile.size();
        int maxnum;
        for(int i=n-1; i>=0; i--){
            std::stringstream ss(im->logfile[i]);
            std::string action;
            ss >> maxnum >> action;
            if(action == "end"){
                break;
            }
        }
        std::vector<int> nums;
        for(int i=n-1; i>=0; i--){
            std::stringstream ss(im->logfile[i]);
            std::string action;
            int num;
            ss >> num;
            if(num < maxnum-50){
                ss >> action;
                if(action == "end"){
                    nums.push_back(num);
                }
                else if(action == "create"){
                    bool del = true;
                    for(uint32_t j=0; j<nums.size(); j++){
                        if(nums[j] == num){
                            del = false;
                            break;
                        }
                    }
                    if(del){
                        extent_protocol::extentid_t id;
                        ss >> id;
                        im->free_inode(id);
                    }
                }
                im->logfile.erase(im->logfile.begin()+i);
            }
        }
        pthread_mutex_unlock(&im->loglock);
    }
    return NULL;
}

void inode_manager::commit(){
    std::stringstream ss;
    ss << transnum << " commit " << version++;
    logfile.push_back(ss.str());
}

void inode_manager::undo(){
    pthread_mutex_lock(&loglock);
    uint32_t n=logfile.size();
    if(logpos == 0){
        logpos = n-1;
    }
    for(; logpos>=0; logpos--){
        std::stringstream ss;
        uint32_t tnum, vnum;
        std::string action;
        ss << logfile[logpos];
        ss >> tnum >> action;
        if(action == "commit"){
            ss >> vnum;
            if(vnum == version-1){
                version--;
                break;
            }
        }
        else if(action == "create"){
            extent_protocol::extentid_t id;
            ss >> id;
            remove_file(id);
        }
        else if(action == "put"){
            extent_protocol::extentid_t id;
            uint32_t size;
            ss >> id >> size;
            for(uint32_t j=0; j<size+2; j++)
                ss.get();
            ss >> size;
            ss.get();
            char cbuf[size];
            for(uint32_t j=0; j<size; j++)
                ss.get(cbuf[j]);
            write_file(id, cbuf, size);
        }
        else if(action == "remove"){
            extent_protocol::extentid_t id;
            uint32_t size;
            ss >> id >> size;
            ss.get();
            char cbuf[size];
            for(uint32_t j=0; j<size; j++)
                ss.get(cbuf[j]);
            alloc_num[id] = 1;
            write_file(id, cbuf, size);
        }
    }
    pthread_mutex_unlock(&loglock);
}

void inode_manager::redo(){
    pthread_mutex_lock(&loglock);
    uint32_t n=logfile.size();
    for(; logpos<n; logpos++){
        std::stringstream ss;
        uint32_t tnum, vnum;
        std::string action;
        ss << logfile[logpos];
        ss >> tnum >> action;
        if(action == "commit"){
            ss >> vnum;
            if(vnum == version+1){
                version++;
                break;
            }
        }
        else if(action == "create"){
            extent_protocol::extentid_t id;
            ss >> id;
            alloc_num[id] = 1;
            write_file(id, "", 0);
        }
        else if(action == "put"){
            extent_protocol::extentid_t id;
            uint32_t size;
            std::string buf;
            ss >> id >> size;
            ss.get();
            char cbuf[size];
            for(uint32_t j=0; j<size; j++)
                ss.get(cbuf[j]);
            write_file(id, cbuf, size);
        }
        else if(action == "remove"){
            extent_protocol::extentid_t id;
            ss >> id;
            remove_file(id);
        }
    }
    pthread_mutex_unlock(&loglock);
}
