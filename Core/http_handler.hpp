#ifndef _HTTP_HANDLER_HPP
#define _HTTP_HANDLER_HPP

#include <Common/task_base.hpp>
#include <Common/base_nio_handler.hpp>
#include <Common/wait_list.hpp>
#include <vector>
#include <netinet/in.h>
#include <tr1/unordered_map>
#include <pthread.h>
#include "http_acceptor.hpp"
#include "worker.hpp"
#include "HttpResponse.hpp"
#include <openssl/aes.h>


class http_handler : public task_base , public base_nio_handler , public http_acceptor { 
public:
	http_handler(int epoll_cnt = 4, int max_conn = 2048);
	~http_handler();
	virtual int open(int port, int handler_thread_num, int thread_stack_size);
	virtual int stop();
	virtual int svc();
	int submitWorker(Worker* worker);

protected:
	// parse http request from mobile ime
	int parseHttpReq(std::string http_header, char* body, int body_len, char* decompress_buf, Worker* worker);
	// send reply back to mobile ime
	int sendReply(int fd, const char* res, int res_len, bool is_compress = false, httpserver::status_code code = httpserver::SCCNT);
	// generate reply
	int genReply(Worker* worker, std::string& reply);
	// generate normal http header
	int genHttpHeader(std::string & header, int length, bool is_compress = false, httpserver::status_code code = httpserver::SCCNT);
	// parser parameters in ime request
	int parseParameter(const std::string& pair, std::string& key, std::string& value, Worker* worker);
	// decode post body
	int decodeReqBody(char* body, int body_len, char* decompress_buf, std::string& result, bool is_compress = false);
	int handle_recv(int fd, int epoll_fd_idx, char* recv_buf, char* req_buf, char* decompress_buf, void* args, const std::string &debug_file, bool debug_on);
    // encode using aes
    int aes_encode(const unsigned char* src, int src_len, unsigned char* dest);

protected:
	wait_list_t<Worker, &Worker::task_list_node> m_task_list;
	int tcp_connect_timeout;
    std::string padding_str[AES_BLOCK_SIZE];
		
};

#endif
