#include <iostream>
#include <dlfcn.h>
#include <unistd.h>
#include <fcntl.h>
#include <cerrno>
#include <cstring>
#include <cstdarg>
#include <cstdlib>
#include <unordered_map>
#include "cache.h"


typedef int (*open_func)(const char *pathname, int flags, ...);

typedef ssize_t (*read_func)(int fd, void *buf, size_t count);

typedef ssize_t (*write_func)(int fd, const void *buf, size_t count);

typedef int (*close_func)(int fd);

typedef off_t (*lseek_func)(int fd, off_t offset, int whence);

typedef int (*fsync_func)(int fd);


static BlockCache *g_cache = nullptr;
const size_t BLOCK_SIZE = 4096;


BlockCache::BlockCache(size_t cache_size) : cache_size_(cache_size), aio_context_() {
}


BlockCache::~BlockCache() {
    flushAllDirtyBlocks();
}

void BlockCache::flushAllDirtyBlocks() {
    for (auto &block: cache_list_) {
        if (block.dirty) {
            flushBlock(block);
        }
    }
}

void BlockCache::flushBlock(CacheBlock &block) {
    ssize_t bytesWritten = pwrite(block.fd, block.data.data(), block.data.size(), block.offset);

    if (bytesWritten == -1) {
        std::cerr << "Error in pwrite: " << strerror(errno) << std::endl;
    } else {
        block.dirty = false;
    }
}


file_descriptor_t BlockCache::openFile(const std::string &path, int flags, int mode) {
    static open_func original_open = nullptr;
    if (!original_open) {
        original_open = (open_func) dlsym(RTLD_NEXT, "open");
        if (!original_open) {
            std::cerr << "dlsym error: " << dlerror() << std::endl;
            errno = EIO;
            return -1;
        }
    }
    return original_open(path.c_str(), flags & ~O_DIRECT, mode);
}

int BlockCache::closeFile(file_descriptor_t fd) {
    flushDirtyBlocksForFd(fd);
    for (auto it = cache_list_.begin(); it != cache_list_.end();) {
        if (it->fd == fd) {
            cache_map_.erase({fd, it->offset});
            it = cache_list_.erase(it);
        } else {
            ++it;
        }
    }
    if (::close(fd) == -1) {
        std::cerr << "Ошибка закрытия файла: " << fd << " - " << strerror(errno)
                  << std::endl;
        return -1;
    }
    return 0;
}

ssize_t BlockCache::read(file_descriptor_t fd, void *buf, size_t count) {
    off_t current_offset = lseek(fd, 0, SEEK_CUR);
    if (current_offset == -1) {
        std::cerr << "Error getting current offset: " << strerror(errno) << std::endl;
        return -1;
    }

    size_t bytes_read = 0;
    while (bytes_read < count) {
        size_t block_offset = current_offset % BLOCK_SIZE;
        off_t block_start = current_offset - block_offset;
        size_t remaining_bytes = count - bytes_read;
        size_t read_size = std::min(remaining_bytes, BLOCK_SIZE - block_offset);

        CacheBlock *block = getBlock(fd, block_start);
        if (!block) {
            block = loadBlock(fd, block_start);
            if (!block) {
                return -1;
            }
        }

        std::memcpy(static_cast<char *>(buf) + bytes_read, block->data.data() + block_offset, read_size);
        bytes_read += read_size;
        current_offset += read_size;
    }
    lseek(fd, current_offset, SEEK_SET);
    return bytes_read;
}

BlockCache::CacheBlock *BlockCache::getBlock(file_descriptor_t fd, file_offset_t offset) {
    auto it = cache_map_.find({fd, offset});
    if (it != cache_map_.end()) {
        cache_list_.splice(cache_list_.begin(), cache_list_, it->second);
        return &(*cache_list_.begin());
    }
    return nullptr;
}

BlockCache::CacheBlock *BlockCache::loadBlock(file_descriptor_t fd, file_offset_t offset) {
    if (cache_list_.size() >= cache_size_) {
        evictBlock();
    }

    CacheBlock new_block = {fd, offset, std::vector<char>(BLOCK_SIZE), false};

    ssize_t bytesRead = pread(fd, new_block.data.data(), BLOCK_SIZE, offset);
    if (bytesRead == -1) {
        std::cerr << "Error in pread: " << strerror(errno) << std::endl;
        return nullptr;
    }
    new_block.data.resize(bytesRead);

    cache_list_.push_front(new_block);
    auto it = cache_list_.begin();
    cache_map_[{fd, offset}] = it;
    return &(*it);
}

ssize_t BlockCache::write(file_descriptor_t fd, const void *buf, size_t count) {
    off_t current_offset = lseek(fd, 0, SEEK_CUR);
    if (current_offset == -1) {
        std::cerr << "Error getting current offset: " << strerror(errno) << std::endl;
        return -1;
    }

    size_t bytes_written = 0;
    while (bytes_written < count) {
        size_t block_offset = current_offset % BLOCK_SIZE;
        off_t block_start = current_offset - block_offset;
        size_t remaining_bytes = count - bytes_written;
        size_t write_size = std::min(remaining_bytes, BLOCK_SIZE - block_offset);

        CacheBlock *block = getBlock(fd, block_start);
        if (!block) {
            block = loadBlock(fd, block_start);
            if (!block) {
                return -1;
            }
        }

        if (block_offset == 0 && write_size == BLOCK_SIZE) {
            std::memcpy(block->data.data(), static_cast<const char *>(buf) + bytes_written, write_size);
        } else {
            std::memcpy(block->data.data() + block_offset, static_cast<const char *>(buf) + bytes_written, write_size);
        }

        block->dirty = true;
        bytes_written += write_size;
        current_offset += write_size;

    }
    lseek(fd, current_offset, SEEK_SET);
    return bytes_written;
}


int BlockCache::fsync(file_descriptor_t fd) {
    flushDirtyBlocksForFd(fd);

    if (::fsync(fd) == -1) {
        std::cerr << "Error in fsync (fsync): " << strerror(errno) << std::endl;
        return -1;
    }
    return 0;
}

void BlockCache::flushDirtyBlocksForFd(int fd) {
    for (auto it = cache_list_.begin(); it != cache_list_.end();) {
        if (it->fd == fd && it->dirty) {
            flushBlock(*it);
        }
        if (it->fd == fd && !it->dirty) {
            it++;
            continue;
        }
        it++;

    }
}

void BlockCache::evictBlock() {
    if (cache_list_.empty()) {
        return;
    }

    auto last = cache_list_.back();
    if (last.dirty) {
        flushBlock(last);
    }

    cache_map_.erase({last.fd, last.offset});
    cache_list_.pop_back();
}


extern "C" int cache_init(size_t cache_size) {
    if (g_cache != nullptr) {
        std::cerr << "Cache already initialized." << std::endl;
        return -1;
    }
    g_cache = new BlockCache(cache_size);
    std::cout << "Cache initialized with size: " << cache_size << std::endl;
    return 0;
}

extern "C" void cache_destroy() {
    if (g_cache != nullptr) {
        delete g_cache;
        g_cache = nullptr;
        std::cout << "Cache destroyed." << std::endl;
    }
}

extern "C" int open(const char *pathname, int flags, ...) {
    static open_func original_open = nullptr;
    if (!original_open) {
        original_open = (open_func) dlsym(RTLD_NEXT, "open");
        if (!original_open) {
            std::cerr << "dlsym error: " << dlerror() << std::endl;
            errno = EIO;
            return -1;
        }
    }
    va_list args;
    va_start(args, flags);
    int mode = 0;
    if (flags & O_CREAT) {
        mode = va_arg(args, int);
    }
    va_end(args);


    if (g_cache == nullptr) {
        return original_open(pathname, flags, mode);
    }
    return g_cache->openFile(pathname, flags, mode);
}



extern "C" ssize_t read(int fd, void *buf, size_t count) {
    static read_func original_read = nullptr;
    if (!original_read) {
        original_read = (read_func) dlsym(RTLD_NEXT, "read");
        if (!original_read) {
            std::cerr << "dlsym error: " << dlerror() << std::endl;
            errno = EIO;
            return -1;
        }
    }
    if (g_cache == nullptr) {
        return original_read(fd, buf, count);
    }
    return g_cache->read(fd, buf, count);
}


extern "C" ssize_t write(int fd, const void *buf, size_t count) {
    static write_func original_write = nullptr;
    if (!original_write) {
        original_write = (write_func) dlsym(RTLD_NEXT, "write");
        if (!original_write) {
            std::cerr << "dlsym error: " << dlerror() << std::endl;
            errno = EIO;
            return -1;
        }
    }
    if (g_cache == nullptr) {
        return original_write(fd, buf, count);
    }
    return g_cache->write(fd, buf, count);
}


extern "C" int close(int fd) {
    static close_func original_close = nullptr;
    if (!original_close) {
        original_close = (close_func) dlsym(RTLD_NEXT, "close");
        if (!original_close) {
            std::cerr << "dlsym error: " << dlerror() << std::endl;
            errno = EIO;
            return -1;
        }
    }
    if (g_cache == nullptr) {
        return original_close(fd);
    }


    int result = g_cache->closeFile(fd);
    if (result != 0) {
        return result;
    }
    return original_close(fd);
}
extern "C" off_t lseek(int fd, off_t offset, int whence) {
    static lseek_func original_lseek = nullptr;
    if (!original_lseek) {
        original_lseek = (lseek_func) dlsym(RTLD_NEXT, "lseek");
        if (!original_lseek) {
            std::cerr << "dlsym error: " << dlerror() << std::endl;
            errno = EIO;
            return -1;
        }
    }
    return original_lseek(fd, offset, whence);
}


extern "C" int fsync(int fd) {
    static fsync_func original_fsync = nullptr;
    if (!original_fsync) {
        original_fsync = (fsync_func) dlsym(RTLD_NEXT, "fsync");
        if (!original_fsync) {
            std::cerr << "dlsym error: " << dlerror() << std::endl;
            errno = EIO;
            return -1;
        }
    }
    if (g_cache == nullptr) {
        return original_fsync(fd);
    }
    return g_cache->fsync(fd);
}
