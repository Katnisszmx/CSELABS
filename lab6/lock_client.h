// lock client interface.

#ifndef lock_client_h
#define lock_client_h

#include <string>
#include "lock_protocol.h"
#include "rpc.h"
#include <vector>
#include <map>


// Client interface to the lock server
class lock_client {
    protected:
        rpcc *cl;
    public:
        lock_client(std::string d);
        virtual ~lock_client() {};
        virtual lock_protocol::status acquire(lock_protocol::lockid_t);
        virtual lock_protocol::status release(lock_protocol::lockid_t);
        virtual lock_protocol::status stat(lock_protocol::lockid_t);
    private:
        static void* heartbeat(void* arg);
        std::map<lock_protocol::lockid_t, pthread_t> ltmap;
};

struct args{
    lock_client* lcp;
    lock_protocol::lockid_t lid;
};

#endif
