#pragma once

#include "CommonTypes.h"
#include <vector>
#include <memory>
#include <mutex>
#include <atomic>

template <typename T>
class Maybe{
	bool initialized;
	T data;
public:
	Maybe(): initialized(false), data(){}
	Maybe(const Maybe<T> &b){
		*this = b;
	}
	Maybe(const T &b){
		*this = b;
	}
	const Maybe<T> &operator=(const Maybe<T> &b){
		this->initialized = b.initialized;
		this->data = b.data;
		return *this;
	}
	const Maybe<T> &operator=(const T &b){
		this->initialized = true;
		this->data = b;
		return *this;
	}
	const T &operator*() const{
		return this->value();
	}
	const T *operator->() const{
		return &this->value();
	}
	const T &value() const{
		if (!this->initialized)
			throw std::runtime_error("!Maybe<T>::is_initialized()");
		return this->data;
	}
	const T &value_or(const T &v) const{
		if (!this->initialized)
			return v;
		return this->data;
	}
	bool is_initialized() const{
		return this->initialized;
	}
	void clear(){
		this->initialized = false;
	}
};

constexpr unsigned bit(unsigned i){
	return 1U << i;
}

template <typename T, size_t N>
size_t array_size(T (&)[N]){
	return N;
}

template <typename T1, typename T2>
bool check_flag(const T1 &flag, const T2 &mask){
	return (flag & mask) == mask;
}

template <std::uint32_t N, int M>
struct bits_from_u32_helper{
	static const byte_t value = bits_from_u32_helper<N, M + 1>::value |
		(!!((N >> (4 * M)) & 0xF) << M);
};

template <std::uint32_t N>
struct bits_from_u32_helper<N, 8>{
	static const byte_t value = 0;
};

template <std::uint32_t N>
struct bits_from_u32{
	static const byte_t value = bits_from_u32_helper<N, 0>::value;
};

template <typename T>
std::unique_ptr<T> copy_to_unique(const T &x){
	return std::make_unique<T>(x);
}

inline int sign_extend(byte_t n){
	if (n < 0x80)
		return n;
	return 0x100 - (int)n;
}

inline byte_t reduce_sign(int n){
	while (n < 0)
		n += 0x100;
	return n % 256;
}

template <typename T>
typename std::enable_if<!std::is_unsigned<T>::value, int>::type
sign_extend(T n){
	return n;
}

template <typename T, size_t N>
size_t array_length(const T (&)[N]){
	return N;
}

template <typename T, size_t N>
void fill_array(T (&array)[N], byte_t value = 0){
	memset(array, value, sizeof(array));
}
