#include"io_buffer.h"
#include<pthread.h>

io_buffer* buffer_pool::alloc(int N) {
	int index;
	if (N <= u4K)
		index = u4K;
	else if (N <= u16K)
		index = u16K;
	else if (N <= u64K)
		index = u64K;
	else if (N <= u256K)
		index = u256K;
	else if (N <= u1M)
		index = u1M;
	else if (N <= u4M)
		index = u4M;
	else if (N <= u8M)
		index = u8M;
	else
		return NULL;
	pthread_mutex_lock(_mutex);
	if(!_pool[index]) {
		if (_total_mem + index / 1024 >= EXTRA_MEM_LIMIT){
			exit_log("use too many memory");
			::exit(1);
		}
		io_buffer *new_buff = new io_buffer(index);
		exit_if(new_buf == NULL, "new io_buffer");
		_total_mem += index / 1024;
		::pthread_mutex_unlock(_mutex);
		return new_buf;
	}
	io_buffer *target = _pool[index];
	_pool[index] = target->next;
	::pthread_mutex_unlock(_mutex);
	target->next = NULL;
	return target;
}

void buffer_pool::revert(io_buffer* buffer) {
	int index = buffer->capacity;
	buffer->length = 0;
	buffer->head = 0;
	::pthread_mutex_lock(_mutex);
	assert(_pool.find(index) != _pool.end())
	buffer->next = _pool[index]->next;
	_pool[index] = buff;
	::pthread_mutex_unlock(_mutex);
}

void tcp_buffer::pop(int len) {
	assert(_buf != NULL && len <= _buf->length);
	_buf->pop(len);
	if(!_buf->length) {
		buffer_pool::ins()->revert(_buf);
		_buf = NULL;
	}
}

void tcp_buffer::clear() {
	if(_buf) {
		_buf->clear();
		buffer_pool::ins()->revert(_buf);
	}
}

//注意点 
//1、_buf可能没被分配
//2、分配了但是大小可能不够 产生这个原因也是有两种情况，一是上次读取的没有被消费完？？二是这次接受的数据比上次多。
//3、读的时候，开始的位置不是head处，而是length处。
int input_buffer::read_data(int fd) {
	//一次性读出来所有数据
    int rn, ret;
    if (::ioctl(fd, FIONREAD, &rn) == -1)
    {
        error_log("ioctl FIONREAD");
        return -1;
    }
	if(!_buf) {
		_buf = buffer_pool::ins()->alloc(rn);
		if (!_buf)
        {
            error_log("no idle for alloc io_buffer");
            return -1;            
        }
	}else {
		assert(_buf->head == 0);//这里为什么必须是要为0
		if(_buf->capacity < rn + _buf->length) {
			io_buffer *new_buf = new io_buffer(rn + _buf->length);
			if (!new_buf)
			{
				error_log("no idle for alloc io_buffer");
				return -1;            
			}
			new_buf->copy(_buf);
			buffer_pool::ins()->revert(_buf);
			_buf = new_buf;
		}
	}
	do {
		ret = read(fd, _buf->data + _buf->length, rn);
	}while(ret == -1 && errno == EINTR);
	if (ret > 0)
    {
        assert(ret == rn);
        _buf->length += ret;
    }
    return ret;
}

int input_buffer::adjust() {
	if(_buf) {
		_buf->adjust();
	}
}

void output_buffer::adjust() {
	if(_buf) {
		_buf->adjust();
	}
}

//每次用户往用户缓冲写数据的时候都是缓冲的开头开始写。
//用户从缓冲写到内核tcp发送缓冲的时候，写完需要进行以下跳转，使得head为0；
//在一个线程中执行的，不存在并发。
int output_buffer::write_fd(int fd) {
	assert(_buf && _buf->head == 0);//每次写完后
	int writed;
	do {
		writed = ::write(fd, _buf->data + _buf->head, _buf->length);
	}while(writed == -1 && error == EINTR);
	if(writed > 0) { //这里必须要做判断啊，要是ret == -1呢
		_buf->pop(ret);
		_buf->adjust();
	}
	if(writed == -1 && errno == EAGAIN) {
		writed = 0;
	}
	return writed;
}
int output_buffer::send_data(const char *data, int datalen) {
	if(!_buf) {
		_buf = buffer_pool:ins()->alloc(datalen);
		if (!_buf)
        {
            error_log("no idle for alloc io_buffer");
            return -1;
        }
    }
	}else {
		assert(_buf->head == 0); 
		if(_buf->capacity < datalen + _buf->length) {
			io_buffer *new_buffer = buffer_pool::ins()->alloc(datalen + _buf->length);
			f (!new_buf)
            {
                error_log("no idle for alloc io_buffer");
                return -1;
            }
			new_buffer->copy(_buf);
			buffer_pool::ins()->revert(_buf);
			_buf = _new_buf;
		}
	}
	::memcpy(_buf->data + _buf->length, data, datalen);
	_buf->length += datalen;
	return 0;
}
