#pragma once
#include <stddef.h>
#include <stdint.h>

// Synchronization is provided by the caller. No allocation and no waiting.
struct Xu4UartQueue {
    static constexpr size_t capacity = 8192;
    char data[capacity];
    size_t head = 0, tail = 0, used = 0, peak = 0;
    uint64_t dropped = 0, sent = 0;
    void push(const char *p, size_t n) {
        // Reject the entire driver chunk on overflow, preserving earlier bytes.
        if (n > capacity - used) { dropped += n; return; }
        for (size_t i = 0; i < n; ++i) {
            data[head] = p[i]; head = (head + 1) % capacity;
        }
        used += n;
        if (used > peak) peak = used;
    }
    template<class Ready, class Send> unsigned drain(Ready ready, Send send) {
        unsigned n = 0;
        while (n < 16 && used && ready()) {
            send(data[tail]); tail = (tail + 1) % capacity;
            --used; ++sent; ++n;
        }
        return n;
    }
};
