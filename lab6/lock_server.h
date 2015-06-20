// this is the lock server
// the lock client has a similar interface

#ifndef lock_server_h
#define lock_server_h

#include <string>
#include "lock_protocol.h"
#include "lock_client.h"
#include "rpc.h"
#include <map>

struct ctpair{
    int clt;
    int time;
};

class lock_server {
    protected:
        int nacquire;

    public:
        lock_server();
        ~lock_server();
        lock_protocol::status stat(int clt, lock_protocol::lockid_t lid, int &);
        lock_protocol::status acquire(int clt, lock_protocol::lockid_t lid, int &);
        lock_protocol::status release(int clt, lock_protocol::lockid_t lid, int &);
        lock_protocol::status heartbeat(int clt, lock_protocol::lockid_t lid, int &);
        static void* timer(void* arg);
    private:
        const static int lifetime = 3;
        pthread_t tid;
        std::map<lock_protocol::lockid_t, ctpair> locktime;
        pthread_mutex_t mtx;
};

#endif







