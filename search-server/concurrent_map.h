#pragma once

#include "basic_iterator.h"

#include <cstdlib>
#include <map>
#include <numeric>
#include <string>
#include <vector>
#include <mutex>
#include <algorithm>

template <typename Key, typename Value>
class ConcurrentMap {
	typedef typename std::vector<std::map<Key, Value>>::iterator ExternalIterator;
	typedef typename std::map<Key, Value>::iterator InternalIterator;

	using BasicIterator = BasicIterator<std::pair<const Key, Value>, InternalIterator, ExternalIterator>;

public:

	static_assert(std::is_integral_v<Key>, "ConcurrentMap supports only integer keys");

	struct Access {
		std::lock_guard<std::mutex> guard;
		Value& ref_to_value;

		Access(std::mutex& value_mutex, std::map<Key, Value>& map, const Key& key) : guard(value_mutex), ref_to_value(map[key]) {}
	};

	explicit ConcurrentMap(size_t bucket_count) : maps_(bucket_count), mutex_values_(bucket_count) {
	}

	Access operator[](const Key& key) {
		uint64_t ukey = key;
		size_t index = ukey % maps_.size();

		return Access(mutex_values_[index], maps_[index], key);
	}

	void erase(const Key& key) {
		uint64_t ukey = key;
		size_t index = ukey % maps_.size();

		std::lock_guard guard(mutex_values_[index]);
		maps_[index].erase(key);
	}

	std::map<Key, Value> BuildOrdinaryMap() {
		std::map<Key, Value> result;

		for (size_t i = 0; i < maps_.size(); ++i) {
			std::lock_guard guard(mutex_values_[i]);
			result.insert(maps_[i].begin(), maps_[i].end());
		}

		return result;
	}

	BasicIterator begin() {
		return BasicIterator(maps_.begin(), maps_.end());
	}

	BasicIterator end() {
		return BasicIterator(maps_.end(), maps_.end());
	}

private:
	std::vector<std::map<Key, Value>> maps_;
	std::vector<std::mutex> mutex_values_;
};