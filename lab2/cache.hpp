#ifndef CACHE_H
#define CACHE_H

#include <fcntl.h>
#include <list>
#include <map>
#include <string>
#include <vector>
#include <libaio.h>
#include <cstdlib>
#include <cstdint>
#include <iostream>
#include <unordered_map>

using file_descriptor_t = int;
using file_offset_t = off_t;

namespace detail {

    inline uint64_t XXH3_64bits(const void *key, size_t len) {
        const uint8_t *p = static_cast<const uint8_t *>(key);
        const uint8_t *const bEnd = p + len;
        uint64_t h64 = len;


        while (p + 8 <= bEnd) {
            uint64_t k1 = *(const uint64_t *) p;
            h64 ^= k1;
            h64 = (h64 * 11400714819323198485ULL) + 1;
            p += 8;
        }


        while (p < bEnd) {
            h64 ^= *p;
            h64 = (h64 * 11400714819323198485ULL) + 1;
            p++;
        }


        h64 ^= h64 >> 33;
        h64 *= 0xff51afd7ed558ccdULL;
        h64 ^= h64 >> 33;
        h64 *= 0xc4ceb9fe1a85ec53ULL;
        h64 ^= h64 >> 33;

        return h64;
    }


    struct pair_hash {
        template<class T1, class T2>
        size_t operator()(const std::pair <T1, T2> &p) const {
            char combined[sizeof(T1) + sizeof(T2)];
            memcpy(combined, &p.first, sizeof(T1));
            memcpy(combined + sizeof(T1), &p.second, sizeof(T2));


            return XXH3_64bits(combined, sizeof(combined));
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
    std::list <CacheBlock> cache_list_;
    std::unordered_map <std::pair<int, off_t>, std::list<CacheBlock>::iterator, detail::pair_hash> cache_map_;
    io_context_t aio_context_;
};

extern "C" int cache_init(size_t cache_size);
extern "C" void cache_destroy();

#endif
