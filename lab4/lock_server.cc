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
    lock_protocol::status ret = lock_protocol::OK;
    // Your lab4 code goes here
    if (locktable.find(lid) != locktable.end()) {
        pthread_mutex_lock(&mtx);
        while(locktable[lid] == LOCKED){
            pthread_mutex_unlock(&mtx);
            pthread_mutex_lock(&mtx);
        }
        locktable[lid] = LOCKED;
        pthread_mutex_unlock(&mtx);
    }
    else {
        locktable.insert(std::pair<lock_protocol::lockid_t, state>(lid, LOCKED));
    }
    return ret;
}

lock_protocol::status lock_server::release(int clt, lock_protocol::lockid_t lid, int &r)
{
    lock_protocol::status ret = lock_protocol::OK;
    // Your lab4 code goes here
    if (locktable.find(lid) != locktable.end()) {
        locktable[lid] = FREE;
    }
    else {
        return lock_protocol::NOENT;
    }
    return ret;
}
