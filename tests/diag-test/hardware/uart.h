#pragma once
constexpr int uart_default=0;
bool uart_is_writable(int);
void uart_putc_raw(int,char);
