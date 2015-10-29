#ifndef WORKER_HPP
#define WORKER_HPP

#include <stdint.h>
#include <unistd.h>
#include <Common/linked_list.hpp>
#include <vector>
#include <sys/time.h>

class Worker {
public:
    uint32_t id;
    std::string uid;
    bool valid;
    int fd;
    linked_list_node_t task_list_node;
    int res_cnt;
    std::string func_name;
    std::string log;
    std::string req_uri;
    timeval recv_timestamp;
    int p;
    int r;
    std::string del_word;
    std::string sign;

public:
    Worker(int _fd) {
        id = 0;
        uid = "";
        fd = _fd;
        valid = true;
        log = "";
        res_cnt = 0;
        func_name = "";
        req_uri = "";
        gettimeofday(&recv_timestamp, NULL);
        p = 0;
        r = 1;
        del_word = "";
        sign = "";
    }
};

#endif
