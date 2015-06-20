// the lock server implementation

#include "lock_server.h"
#include <sstream>
#include <stdio.h>
#include <unistd.h>
#include <arpa/inet.h>

lock_server::lock_server():
    nacquire (0)
{
    pthread_mutex_init(&mtx, NULL);
    pthread_create(&tid, NULL, timer, (void*)this);
}

lock_server::~lock_server(){
    pthread_cancel(tid);
}

lock_protocol::status lock_server::stat(int clt, lock_protocol::lockid_t lid, int &r)
{
    lock_protocol::status ret = lock_protocol::OK;
    printf("stat request from clt %d\n", clt);
    r = nacquire;
    return ret;
}

lock_protocol::status lock_server::acquire(int clt, lock_protocol::lockid_t lid, int &r)
{
    pthread_mutex_lock(&mtx);
    std::map<lock_protocol::lockid_t, ctpair>::iterator iter=locktime.find(lid);
    if (iter == locktime.end()) {
        ctpair pair;
        pair.clt=clt;
        pair.time=0;
        locktime.insert(std::pair<lock_protocol::lockid_t, ctpair>(lid, pair));
    }
    else{
        while(locktime[lid].time >= 0){
            pthread_mutex_unlock(&mtx);
            pthread_mutex_lock(&mtx);
        }
        locktime[lid].clt = clt;
        locktime[lid].time = 0;
    }
    pthread_mutex_unlock(&mtx);
    return lock_protocol::OK;
}

lock_protocol::status lock_server::release(int clt, lock_protocol::lockid_t lid, int &r)
{
    lock_protocol::status ret = lock_protocol::OK;
    pthread_mutex_lock(&mtx);
    std::map<lock_protocol::lockid_t, ctpair>::iterator iter=locktime.find(lid);
    if (iter != locktime.end()) {
        if (iter->second.clt == clt)
            iter->second.time = -1;
    }
    else {
        ret = lock_protocol::NOENT;
    }
    pthread_mutex_unlock(&mtx);
    return ret;
}

lock_protocol::status lock_server::heartbeat(int clt, lock_protocol::lockid_t lid, int &r)
{
    pthread_mutex_lock(&mtx);
    if(locktime[lid].clt == clt && locktime[lid].time >= 0)
        locktime[lid].time = 0;
    pthread_mutex_unlock(&mtx);
    return lock_protocol::OK;
}

void* lock_server::timer(void* arg){
    lock_server* lsp = (lock_server*)arg;
    while(true){
        sleep(1);
        std::map<lock_protocol::lockid_t, ctpair>::iterator iter;
        pthread_mutex_lock(&(lsp->mtx));
        for(iter=lsp->locktime.begin(); iter!=lsp->locktime.end(); iter++){
            if(iter->second.time>=lifetime){
                iter->second.time=-1;
            }
            else if(iter->second.time>=0){
                iter->second.time++;
            }
        }
        pthread_mutex_unlock(&(lsp->mtx));
    }
    pthread_exit(NULL);
}
