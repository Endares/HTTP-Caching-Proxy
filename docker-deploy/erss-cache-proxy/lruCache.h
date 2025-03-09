#include <iostream>
#include <list>
#include <unordered_map>
#include<mutex>
#ifndef LRU_CACHE
#define LRU_CACHE
template <typename K, typename V>
class LRUCache {
public:
    LRUCache(size_t capacity) : capacity_(capacity) {}

    V get(const K& key) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = cache_map_.find(key);
        if (it == cache_map_.end()) {
            // Key not found in cache
            return V();
        }

        // Move the accessed item to the front of the list (most recently used)
        cache_list_.splice(cache_list_.begin(), cache_list_, it->second);

        // Return the value associated with the key
        return it->second->second;
    }

    void put(const K& key, const V& value) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = cache_map_.find(key);
        if (it != cache_map_.end()) {
            // Key already exists, update the value and move to front
            it->second->second = value;
            cache_list_.splice(cache_list_.begin(), cache_list_, it->second);
            return;
        }

        // If the cache is full, remove the least recently used item
        if (cache_list_.size() == capacity_) {
            auto last = cache_list_.back();
            cache_map_.erase(last.first);
            cache_list_.pop_back();
        }

        // Insert the new item at the front of the list
        cache_list_.emplace_front(key, value);
        cache_map_[key] = cache_list_.begin();
    }
    //return the value while don't modify the order
    V peek(const K& key) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = cache_map_.find(key);
        if (it == cache_map_.end()) {
            // Key not found in cache
            return V();
        }

        // Return the value associated with the key without modifying its position
        return it->second->second;
    }

    void print() const {
        std::lock_guard<std::mutex> lock(mutex_);
        for (const auto& item : cache_list_) {
            std::cout << "{" << item.first << ": " << item.second << "} ";
        }
        std::cout << std::endl;
    }

private:
    size_t capacity_;
    std::list<std::pair<K, V>> cache_list_;
    std::unordered_map<K, typename std::list<std::pair<K, V>>::iterator> cache_map_;
protected:
    mutable std::mutex mutex_;
};
#endif

