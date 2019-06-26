#ifndef __IO_BUFFER_H__
#define __IO_BUFFER_H__

#include <list>
#include <string.h>
#include <assert.h>
#include <stdint.h>
#include <pthread.h>
#include <ext/hash_map>

#define EXTRA_MEM_LIMIT (5U * 1024 * 1024) //unit is K, so EXTRA_MEM_LIMIT = 5GB

//io_buffer是从buffer_pool中获取的。
class io_buffer {
	io_buffer(int size):capacity(size), length(0), head(0), next(NULL) {
		data = new char[size];
		assert(data);
	}
	void clear() {
		head = length = 0;
	}
	void adjust() {//move data to head
		if(head && length) {
			::memmove(data,data + head, length);
			head = 0;
		}
	}
	void copy(const io_buffer* other) {
		::memcpy(data, other->data + head, other->length);
		head = 0;
		length = other->length;
	}
	void pop(int len) {
		head += len;
		length -= len;
	}
	int capacity;
	int length;
	int head;
	io_buffer* next;
	char *data;
};
//单例模式
class buffer_pool {
	public:
		enum MEM_CAP {
			u4K = 4096,
			u16K = 16384,
			u64K = 65536,
			u256K = 262144,
			u1M = 1048576,
			u4M = 4194304,
			u8M = 8388608
		};
		//这个函数应该被写成私有的
		static void init() {
			_ins = new buffer_pool();
			assert(_ins);
			::pthread_mutext_init(&_mutex);
		}
		static buffer_pool* ins {
			pthread_once(&_once, init);
			return _ins;
		}
		io_buffer* alloc(int N);
		io_buffer* alloc() {return alloc(u4K);}
		void revert(io_buffer* buffer);
	private:
		/**这三个函数被设置为私有的就是为了不能在其他类或者外部环境中被创建，下面会写一个函数供此类被创建**/
		buffer_pool();
		/**下面这两个函数也要写的，该类中存在函数返回该类的实例**/
		buffer_pool(const buffer_pool&);
		const buffer_pool & operator=(const buffer_pool&);//这里将返回值设置为const buffer_bool &，多加了const，相当与两个赋值运算符函数都没有定义。那不是const的，也是两个都不会定义啊，这里应该是写错了，不过效果一样
		typedef __gnu_cxx::hash_map<int, io_buffer*>pool_t;
		typedef __gnu_cxx::hash_map<int, io_buffer>::iterator pool_it;		
		pool_t _pool;
		uint64_t _total_mem;
		static buffer_bool* _ins;
		static pthread_mutex_t _mutex;
		static pthread_once_t _once;		
		
};

class tcp_buffer {
	protected:
		io_buffer *_buf;
	public:
		tcp_buffer():_buf(NULL){};
		~tcp_buffer() { clear(); };
		const int length() const { return _buf? _buf->length : 0; }
		void pop(int len);
		void clear();
};

class output_buffer : public tcp_buffer {
	public:
		void send_data(const char *data, int datalen);
		int write_fd(int fd);
};

class input_buffer : public tcp_buffer {
	public:
		int read_data(int fd);
		const char *data() const { return _buf? _buf->data + _buf->head : NULL; }
		void adjust();
};

class output_buffer : public tcp_buffer {
	public:
		int write_fd(int fd);
		int send_data(const char *data, int datalen);
		void adjust();
};
#endif