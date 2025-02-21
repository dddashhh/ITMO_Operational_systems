#ifndef CACHE_H
#define CACHE_H

#include <fcntl.h>
#include <list>
#include <map>
#include <string>
#include <vector>
#include <libaio.h>
#include <cstdlib>
#include <iostream>
#include <unordered_map>

typedef int file_descriptor_t;
typedef off_t file_offset_t;

namespace detail {
    struct pair_hash {
        template<class T1, class T2>
        size_t operator()(const std::pair<T1, T2> &p) const {
            auto hash1 = std::hash<T1>{}(p.first);
            auto hash2 = std::hash<T2>{}(p.second);
            if (hash1 != hash2) {
                return hash1 ^ hash2;
            }
            return hash1;
        }
    };
}

class BlockCache {
public:
    explicit BlockCache(size_t cache_size);

    ~BlockCache();

    file_descriptor_t openFile(const std::string &path, int flags, int mode);

    int closeFile(file_descriptor_t fd);

    ssize_t read(int fd, void *buf, size_t count);

    ssize_t write(int fd, const void *buf, size_t count);

    int fsync(file_descriptor_t fd);

    struct CacheBlock {
        int fd;
        off_t offset;
        std::vector<char> data;
        bool dirty;
    };

    CacheBlock *loadBlock(int fd, off_t offset);

    CacheBlock *getBlock(int fd, off_t offset);

    void evictBlock();

    void flushBlock(CacheBlock &block);

    void flushAllDirtyBlocks();

    void flushDirtyBlocksForFd(int fd);

private:
    size_t cache_size_;
    std::list<CacheBlock> cache_list_;
    std::unordered_map<std::pair<int, off_t>, std::list<CacheBlock>::iterator, detail::pair_hash> cache_map_;
    io_context_t aio_context_;
};

extern "C" int cache_init(size_t cache_size);
extern "C" void cache_destroy();

#endif
