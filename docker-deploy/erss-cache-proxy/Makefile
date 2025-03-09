CXX = g++
CXXFLAGS = -std=c++11 -Wall -O2 -g

.PHONY: all clean

all: proxy client lru_test

proxy: proxy.cpp
	$(CXX) $(CXXFLAGS) proxy.cpp -o proxy 

client: client.cpp
	$(CXX) $(CXXFLAGS) client.cpp -o client

lru_test: test_lruCache.cpp
	$(CXX) $(CXXFLAGS) test_lruCache.cpp -o lru_test

clean:
	rm -f proxy client lru_test
