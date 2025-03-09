#include "lruCache.h"
#include"netutil.h"
int main()
{
    LRUCache<int, std::string> cache(3);

    cache.put(1, "one");
    cache.put(2, "two");
    cache.put(3, "three");

    cache.print(); // Output: {3: three} {2: two} {1: one}

    std::cout << "Get 2: " << cache.get(2) << std::endl; // Output: Get 2: two
    cache.print();                                       // Output: {2: two} {3: three} {1: one}

    cache.put(4, "four"); // This will evict key 1
    cache.print();        // Output: {4: four} {2: two} {3: three}
    cache.put(4,"FOUR"); //update value of 4
    cache.print();
    auto three = cache.peek(3);
    std::cout << three << std::endl;
    cache.print();
    cout << get_current_time() << endl;
    return 0;
}
