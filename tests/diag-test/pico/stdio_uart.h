#pragma once
struct stdio_driver_t {
    void (*out_chars)(const char *, int);
    void (*out_flush)();
};
extern stdio_driver_t stdio_uart;
