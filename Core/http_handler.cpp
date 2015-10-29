#include "http_handler.hpp"
#include "http_acceptor.hpp"
#include <string>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/types.h> 
#include <fcntl.h>
#include <zlib.h>
#include <Common/service_log.hpp>

#define max_recv_buf_len (1024 * 1024)
#define decompress_buf_len (10 * 1024 * 1024)
#define http_header_max_length 1024
using namespace httpserver;

typedef struct {
} handle_args;

const std::string RELOAD_UMISBL = "/nav/reload_umis_blacklist";

const int letter_num_map[26] = {2,2,2,3,3,3,4,4,4,5,5,5,6,6,6,7,7,7,7,8,8,8,9,9,9,9};

const static char* aes_key = "8d33ae022be2a180";

http_handler::http_handler(int _epoll_cnt, int _max_conn) : http_acceptor(_epoll_cnt, _max_conn) {
    for(int i=0;i<AES_BLOCK_SIZE;i++) {
        for(int j=AES_BLOCK_SIZE-i;j>0;j--)
            padding_str[i] += (char)(AES_BLOCK_SIZE-i);
    }
}

http_handler::~http_handler() {
}

int http_handler::open(int port, int handler_thread_num, int thread_stack_size) {
	listen_port = port;
	send_timeout = 20;
	recv_timeout = 20;
	for (int i = 0; i < epoll_cnt; i++) {
		epoll_fds[i] = epoll_create(max_conn);
		if (epoll_fds[i] == -1) {
			_ERROR("epoll create err on idx=%d", i);
			return -1;
		}
	}
	for (int i = 0; i < epoll_cnt; i++) {
		if (create_pipe(i) < 0) {
			_ERROR("create pipe err on idx=%d", i);
			return -1;
		}
		add_input_fd(pipe_read_fds[i], epoll_fds[i]);
	}

	if (create_listen(listen_fd, listen_port)) {
		_ERROR("create listen fail on port=%d", listen_port);
		return -1;
	}
	add_input_fd(listen_fd, epoll_fds[0]);

	tcp_connect_timeout = 40;
	_INFO("start thread num=%d", handler_thread_num * epoll_cnt);
	return task_base::open(handler_thread_num * epoll_cnt, thread_stack_size);
}

int http_handler::stop() {
	for (int i = 0; i < epoll_cnt; i++) {
		int stop = 1;
		write(pipe_write_fds[i], &stop, 1);
	}
	stop_task = 1;
	m_task_list.flush();
	join();
	_INFO("http_handler stops");
	return 0;
}

int http_handler::svc() {
	int local_thd_idx = 0;
	int local_epoll_idx = 0;
	pthread_mutex_lock(&epoll_idx_mutex);
	local_epoll_idx = epoll_thread_cnt;
	local_thd_idx = epoll_thread_cnt;
	epoll_thread_cnt++;
	pthread_mutex_unlock(&epoll_idx_mutex);
	local_epoll_idx = local_epoll_idx % epoll_cnt;

	int epoll_fd = epoll_fds[local_epoll_idx];
	epoll_event* es = epoll_ready_events[local_epoll_idx];
	pthread_mutex_t* epoll_mutex = &epoll_mutexes[local_epoll_idx];
	int* event_num = &epoll_ready_event_num[local_epoll_idx];

	char* recv_buf = (char*)malloc(max_recv_buf_len);
	char* req_buf = (char*)malloc(max_recv_buf_len);
	char* decompress_buf = (char*)malloc(decompress_buf_len);
	int fd, new_fd;
	int tmp_conn;
	sockaddr_in sa;
	socklen_t len;

	handle_args* args = new handle_args;

	while (!stop_task) {
		pthread_mutex_lock(epoll_mutex);
		if (stop_task) {
			pthread_mutex_unlock(epoll_mutex);
			break;
		}

		if ((*event_num) <= 0) {
			*event_num = epoll_wait(epoll_fd, es, max_conn, -1);
		}
		if ((*event_num)-- < 0) {
			pthread_mutex_unlock(epoll_mutex);
			if (errno == EINTR) {
				continue;
			}
			else {
				break;
			}
		}
		const std::string debug_file = "";

		fd = es[(*event_num)].data.fd;
		if (fd == listen_fd) {
			del_input_fd(fd, epoll_fd);
			pthread_mutex_unlock(epoll_mutex);
			while ((new_fd = accept(fd, NULL, NULL)) >= 0) {
				pthread_mutex_lock(&conn_mutex);
				conn_num++;
				tmp_conn = conn_num;
				pthread_mutex_unlock(&conn_mutex);
				len = sizeof(sa);
				getpeername(new_fd, (struct sockaddr *)&sa, &len);
				set_socket(new_fd, O_NONBLOCK);
				add_input_fd(new_fd, epoll_fds[incoming_fd_cnt % epoll_cnt]);
				_INFO("new conn on %s:%d,fd=%d,conn_num=%d,epollno=%d", inet_ntoa(sa.sin_addr), ntohs(sa.sin_port), new_fd, tmp_conn, incoming_fd_cnt % epoll_cnt);
				incoming_fd_cnt++;
			}
			add_input_fd(fd, epoll_fd);
		}
		else if (fd == pipe_read_fds[local_epoll_idx]) {
			_INFO("get a stop signal");
			pthread_mutex_unlock(epoll_mutex); 
			continue;
		}
		else {
			del_input_fd(fd, epoll_fd);
			pthread_mutex_unlock(epoll_mutex);
			handle_recv(fd, local_epoll_idx, recv_buf, req_buf, decompress_buf, args, debug_file, false);
		}

	}
	free(recv_buf);
	free(req_buf);
	free(decompress_buf);
	return 0;
}

int http_handler::handle_recv(int fd, int epoll_fd_idx, char* recv_buf, char* req_buf, char* decompress_buf, void* args, const std::string &debug_file, bool debug_on) {
	handle_args* ar = (handle_args*)args;
	int ret = -1;
	Worker* worker = new Worker(fd);
	worker->id = genWid();
	_INFO("[%08X]recv req from fd=%d,epollno=%d", worker->id, fd, epoll_fd_idx);
	std::string http_header;
	bool close_outer_conn;
	int timeout = 20;
	int buf_len;
	std::string reply;
	do {
		reply = "";
		close_outer_conn = false;
		buf_len = max_recv_buf_len;
		ret = i_recv_http_res(fd, recv_buf, buf_len, http_header, timeout * 2);
		if (ret < 0) {
			_ERROR("[%08X] recv req err", worker->id);
			close_outer_conn = true;
			worker->valid = false;
			continue;
		}
		ret = parseHttpReq(http_header, recv_buf, buf_len, decompress_buf, worker);
		if (ret < 0) {
			_ERROR("[%08X] parse req err", worker->id);
			worker->valid = false;
			continue;
		}
	} while (false);

	bool compress = true;
	if (worker->valid) {
		ret = genReply(worker, reply);
		if (ret < 0) {
			_ERROR("[%08X]genReply err", worker->id);
			reply = "";
		}
		status_code code = SCCNT;
        reply = "[\"468\",[\"À—π∑∆¥“Ù ‰»Î∑®\",\"À—π∑À—À˜\",\"À—π∑‰Ø¿¿∆˜\",\"À—π∑±⁄÷Ω\",\"À—π∑ ‰»Î∑®\",\"À—π∑µÿÕº\",\"À—π∑\",\"À—π∑∏ﬂÀŸ‰Ø¿¿∆˜\",\"À—π∑“Ù¿÷\",\"À—π∑ŒÂ±  ‰»Î∑®\"],[\"3\",\"3\",\"3\",\"3\",\"3\",\"3\",\"3\",\"3\",\"3\",\"3\"],[\"\",\"\",\"\",\"\",\"\",\"\",\"\",\"\",\"\",\"\"],[\"\",\"\",\"\",\"\",\"\",\"\",\"\",\"\",\"\",\"\"],[\"0;64;0;0\",\"0;64;0;0\",\"0;64;0;0\",\"0;64;0;0\",\"0;64;0;0\",\"0;64;0;0\",\"0;64;0;0\",\"0;64;0;0\",\"0;64;0;0\",\"0;64;0;0\"],[0],[0],[0],[1],[0,0,0,0,0,0,0,0,0,0]]";
		ret = sendReply(worker->fd, reply.c_str(), reply.length(), compress, code);
		if (ret < 0) {
			close_outer_conn = true;
			_ERROR("[%08X] send reply err", worker->id);
		}
	}
	else if (!worker->valid && !close_outer_conn){
		reply = "";
		status_code code = SCCNT;
		compress = false;
        reply = "[\"468\",[\"À—π∑∆¥“Ù ‰»Î∑®\",\"À—π∑À—À˜\",\"À—π∑‰Ø¿¿∆˜\",\"À—π∑±⁄÷Ω\",\"À—π∑ ‰»Î∑®\",\"À—π∑µÿÕº\",\"À—π∑\",\"À—π∑∏ﬂÀŸ‰Ø¿¿∆˜\",\"À—π∑“Ù¿÷\",\"À—π∑ŒÂ±  ‰»Î∑®\"],[\"3\",\"3\",\"3\",\"3\",\"3\",\"3\",\"3\",\"3\",\"3\",\"3\"],[\"\",\"\",\"\",\"\",\"\",\"\",\"\",\"\",\"\",\"\"],[\"\",\"\",\"\",\"\",\"\",\"\",\"\",\"\",\"\",\"\"],[\"0;64;0;0\",\"0;64;0;0\",\"0;64;0;0\",\"0;64;0;0\",\"0;64;0;0\",\"0;64;0;0\",\"0;64;0;0\",\"0;64;0;0\",\"0;64;0;0\",\"0;64;0;0\"],[0],[0],[0],[1],[0,0,0,0,0,0,0,0,0,0]]";
		ret = sendReply(worker->fd, reply.c_str(), reply.length(), compress, code);
		if (ret < 0) {
			close_outer_conn = true;
			_ERROR("[%08X] send reply err", worker->id);
		}
	}
    long cost = WASTE_TIME_US(worker->recv_timestamp);
    _INFO("[Sogou-Observer,reqid=%08X,cost=%ld,ret=%d,fd=%d,uid=%s,func_name=%s,fulluri=%s,Owner=OP]", worker->id, cost, worker->valid, worker->fd, worker->uid.c_str(), worker->func_name.c_str(), worker->req_uri.c_str());

	if (!close_outer_conn) {
		recycle_fd(fd, epoll_fd_idx);
	}
	else {
		_INFO("[%08X] close outer conn fd=%d", worker->id, fd);
		close_fd(fd);
	}
	return 0;
}

int http_handler::submitWorker(Worker* worker) {
	m_task_list.put(*worker);
	return 0;
}

int http_handler::parseHttpReq(std::string http_header, char* body, int body_len, char* decompress_buf, Worker* worker) {
	size_t s_idx;
	size_t e_idx;
	std::string first_line;
	unsigned int http_version[2];
	char method[16];
	char* uri_buf = (char*)malloc(http_header.length());
	//_INFO("[%08X]header=%s", worker->id, http_header.c_str());
	s_idx = http_header.find("GET");
	if (s_idx == 0) {
		e_idx = http_header.find("\r\n", s_idx);
		if (e_idx == std::string::npos) {
			free(uri_buf);
			return -1;
		}
		first_line = http_header.substr(s_idx, e_idx - s_idx);
	}
	else {
		s_idx = http_header.find("POST");
		if (s_idx == 0) {
			e_idx = http_header.find("\r\n", s_idx);
			if (e_idx == std::string::npos) {
				free(uri_buf);
				return -1;
			}
			first_line = http_header.substr(s_idx, e_idx - s_idx);
		}
		else {
			free(uri_buf);
			return -1;
		}
	}

	if (sscanf(first_line.c_str(), "%s /nav?%s HTTP/%u.%u", method, uri_buf, &http_version[0], &http_version[1]) != 4) {
		_ERROR("[%08X] parse req header err", worker->id);
		free(uri_buf);
		return -1;
	}

	std::string parameters = std::string(uri_buf); 
    worker->req_uri = parameters;
	free(uri_buf);
	size_t last_pos = 0;
	size_t cur_pos = 0;
	std::string key;
	std::string value;
	std::string pair;
	while ((cur_pos = parameters.find("&", last_pos)) != std::string::npos) {
		pair = parameters.substr(last_pos, cur_pos - last_pos);
		last_pos = cur_pos + 1;
		parseParameter(pair, key, value, worker);
	}
	pair = parameters.substr(last_pos);
	parseParameter(pair, key, value, worker);

	/*
	   std::string body_content;
	   decodeReqBody(body, body_len, decompress_buf, body_content, false);
	   _INFO("[%08X]body=%s", worker->id, body_content.c_str());
	   parameters = body_content;
	   last_pos = 0;
	   cur_pos = 0;
	   while ((cur_pos = parameters.find("&", last_pos)) != std::string::npos) {
	   pair = parameters.substr(last_pos, cur_pos - last_pos);
	   last_pos = cur_pos + 1;
	   parseParameter(pair, key, value, worker);
	   }
	   pair = parameters.substr(last_pos);
	   parseParameter(pair, key, value, worker);
	   */

	return 0;
}

int http_handler::sendReply(int fd, const char* res, int res_len, bool is_compress, status_code code) {
	_INFO("reply=%s", res);
	int ret = 0;
	std::string http_header;
	void* compress_buf;
	size_t compress_buf_len;
	size_t content_len = res_len;
	uint32_t old_len = res_len;
	uLong crcval = 0;

	if (code == SC501) {
		_INFO("send 501");
		genHttpHeader(http_header, 0, false, code);
		//_INFO("reply http_header=%s", http_header.c_str());
		if (writen_timeout(fd, http_header.c_str(), http_header.length()) != http_header.length()) {
			return -2;
		}
		return http_header.length();
	}

	if (is_compress && res_len > 0) {
		compress_buf = malloc(res_len * 2 + 10);
		compress_buf_len = res_len * 2 + 10;
		//int gzip_ret = gzip_compress(res, res_len, compress_buf, compress_buf_len);
        int gzip_ret = 0;
		if (gzip_ret < 0) {
			_INFO("gzip_ret fail=%d", gzip_ret);
		}
		content_len = compress_buf_len + 8;
	}

	if (genHttpHeader(http_header, content_len, is_compress) <= 0) {
		if (is_compress && res_len > 0) {
			free(compress_buf);
		}
		return -1;
	}
	//_INFO("reply http_header=%s", http_header.c_str());
	if (writen_timeout(fd, http_header.c_str(), http_header.length()) != http_header.length()) {
		if (is_compress && res_len > 0) {
			free(compress_buf);
		}
		return -2;
	}
	if (res_len > 0) {
		if (!is_compress) {
			if (writen_timeout(fd, res, res_len) != res_len) {
				ret = -1;
			}
			ret = http_header.length() + res_len;
		}
		else {
			//_INFO("TTTT compress_len = %d",compress_buf_len);
			crcval = crc32(crcval, (const Bytef*)res, res_len);
			memcpy(compress_buf + compress_buf_len, &crcval, sizeof(uint32_t));
			memcpy(compress_buf + compress_buf_len + sizeof(uint32_t), &old_len, sizeof(uint32_t));
			compress_buf_len += 8;
			if (writen_timeout(fd, compress_buf, compress_buf_len) != compress_buf_len) {
				ret = -1;
			}
			/*
			   FILE* f = fopen("a.gzip", "wb");
			   fwrite(compress_buf, 1, compress_buf_len, f);
			   fclose(f);
			   */
			ret = http_header.length() + compress_buf_len;
			free(compress_buf);
		}
	}

	return ret;
}

int http_handler::genHttpHeader(std::string & res, int length, bool is_compress, status_code code) {
	char buf[http_header_max_length];

	int s_code = 200;
	std::string status = "OK";
	if (length == 0) {
		s_code = 404;
		status = "Not Found";
	}

	int ret;
	//generate header
	if (code == SC501) {
		ret = snprintf(buf, http_header_max_length, "HTTP/1.1 %s\r\n", status_code_str[code]);
		length = 0;
	}
	else {
		ret = snprintf(buf, http_header_max_length, "HTTP/1.1 %d %s\r\n",s_code,status.c_str());
	}
	res = buf;

	// …˙≥…œÏ”¶±®Œƒ ±º‰¥¡
	time_t t;
	struct tm tmp_time;
	time(&t);
	char timestr[64];
	strftime(timestr, sizeof(timestr), "%a, %e %b %Y %H:%M:%S %Z", gmtime_r(&t, &tmp_time));
	ret = snprintf(buf,http_header_max_length,"Date:%s\r\n",timestr);
	res += buf;

	//add content length
	ret = snprintf(buf,http_header_max_length,"Content-Length:%d\r\n",length);
	res += buf;

	if (length != 0) {
		//add content type
		ret = snprintf(buf,http_header_max_length,"Content-Type: %s; charset=%s\r\n","text","gbk");

		if (is_compress) {
			ret = snprintf(buf, http_header_max_length, "Content-Encoding: gzip\r\n");
			res += buf;
		}
	}

	snprintf(buf,http_header_max_length,"\r\n");
	res += buf;

	return res.length();
}

int http_handler::genReply(Worker* worker, std::string& reply) {
	reply = "";
	return 0;
}

int http_handler::parseParameter(const std::string& pair, std::string& key, std::string& value, Worker* worker) { 
	return 0;
}

int http_handler::decodeReqBody(char* body, int body_len, char* decompress_buf, std::string& result, bool is_compress) {
	return 0;
}


int http_handler::aes_encode(const unsigned char* src, int src_len, unsigned char* dest) {
    return 0;
}
