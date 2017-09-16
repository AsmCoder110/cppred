#include "utility.h"
#include "../common/sha1.h"
#include <sstream>
#include <fstream>
#include <stdexcept>
#include <cctype>

static bool to_unsigned_internal(unsigned &dst, const std::string &s){
	std::stringstream stream(s);
	return !!(stream >> dst);
}

unsigned to_unsigned(const std::string &s){
	unsigned ret;
	if (!to_unsigned_internal(ret, s))
		throw std::runtime_error("Can't convert \"" + s + "\" to integer.");
	return ret;
}

bool hex_no_prefix_to_unsigned_internal(unsigned &dst, const std::string &s){
	std::stringstream stream;
	stream << std::hex << s;
	return !!(stream >> dst);
}

unsigned hex_no_prefix_to_unsigned(const std::string &s){
	unsigned ret;
	if (!hex_no_prefix_to_unsigned_internal(ret, s))
		throw std::runtime_error((std::string)"Can't convert \"" + s + "\" from hex to integer.");
	return ret;
}

std::string hash_buffer(const void *data , size_t size){
	SHA1 sha1;
	sha1.Input(data, size);
	return sha1.ToString();
}

std::string hash_file(const std::string &path){
	std::ifstream file(path, std::ios::binary);
	if (!file)
		throw std::runtime_error("hash_file(): File not found: " + path);

	file.seekg(0, std::ios::end);
	std::vector<unsigned char> data((size_t)file.tellg());
	file.seekg(0);
	file.read((char *)&data[0], data.size());

	return hash_buffer(&data[0], data.size());
}

std::string hash_files(const std::vector<std::string> &files){
	std::vector<unsigned char> data;

	for (auto &path : files){
		std::ifstream file(path, std::ios::binary);
		if (!file)
			throw std::runtime_error("hash_files(): File not found: " + path);

		file.seekg(0, std::ios::end);
		auto n = data.size();
		auto m = (size_t)file.tellg();
		data.resize(n + m);
		file.seekg(0);
		file.read((char *)&data[n], m);
	}

	return hash_buffer(&data[0], data.size());
}

bool check_for_known_hash(const known_hashes_t &known_hashes, const std::string &key, const std::string &value){
	auto it = known_hashes.find(key);
	if (it == known_hashes.end())
		return false;
	return it->second == value;
}

bool is_hex(char c){
	return isdigit(c) || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f');
}

unsigned to_unsigned_default(const std::string &s, unsigned def){
	unsigned ret;
	if (!to_unsigned_internal(ret, s))
		ret = def;
	return ret;
}

unsigned hex_no_prefix_to_unsigned_default(const std::string &s, unsigned def){
	unsigned ret;
	if (!hex_no_prefix_to_unsigned_internal(ret, s))
		ret = def;
	return ret;
}