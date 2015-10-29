
#ifndef _SOCKET_HANDLE_HPP_
#define _SOCKET_HANDLE_HPP_

#include <string>

#define INVALID_FD  -1
class SocketHandle
{
	public:
		SocketHandle(int fd = INVALID_FD, int ref = 0,int flag = 0):_fd(fd),_ref(ref),_flag(flag)
		{
			_read_buf = "";
			pthread_mutex_init(&_mutex, NULL);
		}
		~SocketHandle()
		{
			pthread_mutex_destroy(&_mutex);
		}
		int get_fd()
		{
			return _fd;
		}
		void set_fd(int fd)
		{
			_fd = fd;
		}
		void lock()
		{
			pthread_mutex_lock(&_mutex);
		}
		void unlock()
		{
			pthread_mutex_unlock(&_mutex);
		}
		int acquire()
		{
			return ++_ref;
		}
		int release()
		{
			return --_ref;
		}
		int ref()
		{
			return _ref;
		}
		int get_flag()
		{
			return _flag;
		}
		int set_flag(int flag)
		{
			_flag = flag;
            return _flag;
		}
		std::string & get_read_buf()
		{
			return _read_buf;
		}
	protected:
		int _fd;
		int _ref;
		int _flag;
		pthread_mutex_t _mutex;
		std::string _read_buf;
};
#endif //_SOCKET_HANDLE_HPP_


