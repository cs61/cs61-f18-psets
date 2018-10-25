#ifndef WEENSYOS_LIB_H
#define WEENSYOS_LIB_H
#include "types.h"
#include <new>              // for placement new
#include <type_traits>

// lib.hh
//
//    Functions, constants, and definitions useful in both the kernel
//    and applications.
//
//    Contents: (1) C library subset, (2) system call numbers, (3) console.

extern "C" {
void* memcpy(void* dst, const void* src, size_t n);
void* memmove(void* dst, const void* src, size_t n);
void* memset(void* s, int c, size_t n);
int memcmp(const void* a, const void* b, size_t n);
size_t strlen(const char* s);
size_t strnlen(const char* s, size_t maxlen);
char* strcpy(char* dst, const char* src);
int strcmp(const char* a, const char* b);
int strncmp(const char* a, const char* b, size_t maxlen);
char* strchr(const char* s, int c);
long strtol(const char* s, char** endptr = nullptr, int base = 0);
unsigned long strtoul(const char* s, char** endptr = nullptr, int base = 0);
ssize_t snprintf(char* s, size_t size, const char* format, ...);
ssize_t vsnprintf(char* s, size_t size, const char* format, va_list val);
inline bool isspace(unsigned char c);
}

#define RAND_MAX 0x7FFFFFFF
int rand();
void srand(unsigned seed);
int rand(int min, int max);


// Return the offset of `member` relative to the beginning of a struct type
#ifndef offsetof
#define offsetof(type, member)  __builtin_offsetof(type, member)
#endif

// Return the number of elements in an array
#define arraysize(array)        (sizeof(array) / sizeof(array[0]))


// Type information

// printfmt<T>
//    `printfmt<T>::spec` defines a printf specifier for type T.
//    E.g., `printfmt<int>::spec` is `"d"`.

template <typename T> struct printfmt {};
template <> struct printfmt<bool>           { static constexpr char spec[] = "d"; };
template <> struct printfmt<char>           { static constexpr char spec[] = "c"; };
template <> struct printfmt<signed char>    { static constexpr char spec[] = "d"; };
template <> struct printfmt<unsigned char>  { static constexpr char spec[] = "u"; };
template <> struct printfmt<short>          { static constexpr char spec[] = "d"; };
template <> struct printfmt<unsigned short> { static constexpr char spec[] = "u"; };
template <> struct printfmt<int>            { static constexpr char spec[] = "d"; };
template <> struct printfmt<unsigned>       { static constexpr char spec[] = "u"; };
template <> struct printfmt<long>           { static constexpr char spec[] = "ld"; };
template <> struct printfmt<unsigned long>  { static constexpr char spec[] = "lu"; };
template <typename T> struct printfmt<T*>   { static constexpr char spec[] = "p"; };

template <typename T> constexpr char printfmt<T*>::spec[];


// Min, max, and rounding operations

template <typename T>
inline constexpr T min(T a, T b) {
    return a < b ? a : b;
}
template <typename T>
inline constexpr T max(T a, T b) {
    return b < a ? a : b;
}

template <typename T>
inline constexpr T round_down(T x, unsigned multiple) {
    static_assert(std::is_unsigned<T>::value, "T must be unsigned");
    return x - (x % multiple);
}
template <typename T>
inline constexpr T round_up(T x, unsigned multiple) {
    static_assert(std::is_unsigned<T>::value, "T must be unsigned");
    return round_down(x + multiple - 1, multiple);
}


// msb(x)
//    Return index of most significant one bit in `x`, plus one.
//    Returns 0 if `x == 0`.
inline constexpr int msb(int x) {
    return x ? sizeof(x) * 8 - __builtin_clz(x) : 0;
}
inline constexpr int msb(unsigned x) {
    return x ? sizeof(x) * 8 - __builtin_clz(x) : 0;
}
inline constexpr int msb(long x) {
    return x ? sizeof(x) * 8 - __builtin_clzl(x) : 0;
}
inline constexpr int msb(unsigned long x) {
    return x ? sizeof(x) * 8 - __builtin_clzl(x) : 0;
}
inline constexpr int msb(long long x) {
    return x ? sizeof(x) * 8 - __builtin_clzll(x) : 0;
}
inline constexpr int msb(unsigned long long x) {
    return x ? sizeof(x) * 8 - __builtin_clzll(x) : 0;
}

// lsb(x)
//    Return index of least significant one bit in `x`, plus one.
//    Returns 0 if `x == 0`.
inline constexpr int lsb(int x) {
    return __builtin_ffs(x);
}
inline constexpr int lsb(unsigned x) {
    return __builtin_ffs(x);
}
inline constexpr int lsb(long x) {
    return __builtin_ffsl(x);
}
inline constexpr int lsb(unsigned long x) {
    return __builtin_ffsl(x);
}
inline constexpr int lsb(long long x) {
    return __builtin_ffsll(x);
}
inline constexpr int lsb(unsigned long long x) {
    return __builtin_ffsll(x);
}

// round_down_pow2(x)
//    Round x down to the nearest power of 2.
template <typename T>
inline constexpr T round_down_pow2(T x) {
    static_assert(std::is_unsigned<T>::value, "T must be unsigned");
    return x ? T(1) << (msb(x) - 1) : 0;
}

// round_up_pow2(x)
//    Round x up to the nearest power of 2.
template <typename T>
inline constexpr T round_up_pow2(T x) {
    static_assert(std::is_unsigned<T>::value, "T must be unsigned");
    return x ? T(1) << msb(x - 1) : 0;
}


// Character traits

inline bool isspace(unsigned char ch) {
    return (ch >= '\t' && ch <= '\r') || ch == ' ';
}


// Checksums

uint32_t crc32c(uint32_t crc, const void* buf, size_t sz);
inline uint32_t crc32c(const void* buf, size_t sz) {
    return crc32c(0, buf, sz);
}


// System call numbers: the `syscall` instruction initiates a system
// call. The system call number `SYSCALL_XXX` is in the `%rax`
// register, and other arguments are arranged according to the system
// call calling convention.

#define INT_SYSCALL             48

#define SYSCALL_GETPID          1
#define SYSCALL_YIELD           2
#define SYSCALL_PANIC           3
#define SYSCALL_PAGE_ALLOC      4
#define SYSCALL_FORK            5
#define SYSCALL_EXIT            6


// Console printing

#define CPOS(row, col)  ((row) * 80 + (col))
#define CROW(cpos)      ((cpos) / 80)
#define CCOL(cpos)      ((cpos) % 80)

#define CONSOLE_COLUMNS 80
#define CONSOLE_ROWS    25
extern uint16_t console[CONSOLE_ROWS * CONSOLE_COLUMNS];

// current position of the cursor (80 * ROW + COL)
extern int cursorpos;

// console_clear
//    Erases the console and moves the cursor to the upper left (CPOS(0, 0)).
void console_clear();

#define COLOR_ERROR 0xC000

// console_printf(cursor, color, format, ...)
//    Format and print a message to the x86 console.
//
//    The `format` argument supports some of the C printf function's escapes:
//    %d (to print an integer in decimal notation), %u (to print an unsigned
//    integer in decimal notation), %x (to print an unsigned integer in
//    hexadecimal notation), %c (to print a character), and %s (to print a
//    string). It also takes field widths and precisions like '%10s'.
//
//    The `cursor` argument is a cursor position, such as `CPOS(r, c)` for
//    row number `r` and column number `c`.
//
//    The `color` argument is the initial color used to print. 0x0700 is a
//    good choice (grey on black). The `format` escape %C changes the color
//    being printed.  It takes an integer from the parameter list.
//
//    Returns the final position of the cursor.
int console_printf(int cpos, int color, const char* format, ...)
    __attribute__((noinline));

// console_vprintf(cpos, color, format val)
//    The vprintf version of console_printf.
int console_vprintf(int cpos, int color, const char* format, va_list val)
    __attribute__((noinline));

// Helper versions that default to printing white-on-black at the cursor.
void console_printf(int color, const char* format, ...)
    __attribute__((noinline));
void console_printf(const char* format, ...)
    __attribute__((noinline));

// Generic print library

typedef struct printer printer;
struct printer {
    void (*putc)(printer* p, unsigned char c, int color);
};

void printer_vprintf(printer* p, int color, const char* format, va_list val);


// error_printf(cursor, color, format, ...)
//    Like `console_printf`, but `color` defaults to `COLOR_ERROR`, and
//    in the kernel, the message is also printed to the log.
int error_printf(int cpos, int color, const char* format, ...)
    __attribute__((noinline, cold));
int error_vprintf(int cpos, int color, const char* format, va_list val)
    __attribute__((noinline, cold));
void error_printf(int color, const char* format, ...)
    __attribute__((noinline, cold));
void error_printf(const char* format, ...)
    __attribute__((noinline, cold));


// Assertions

// assert(x)
//    If `x == 0`, print a message and fail.
#define assert(x)           do {                                        \
        if (!(x)) {                                                     \
            assert_fail(__FILE__, __LINE__, #x);                        \
        }                                                               \
    } while (0)
void __attribute__((noinline, noreturn, cold))
assert_fail(const char* file, int line, const char* msg);


// assert_[eq, ne, lt, le, gt, ge](x, y)
//    Like `assert(x OP y)`, but also prints the values of `x` and `y` on
//    failure.
#define assert_op(x, op, y) do {                                        \
        auto __x = (x); auto __y = (y);                                 \
        using __t = typename std::common_type<typeof(__x), typeof(__y)>::type; \
        if (!(__x op __y)) {                                            \
            assert_op_fail<__t>(__FILE__, __LINE__, #x " " #op " " #y,  \
                                __x, #op, __y);                         \
        } } while (0)
#define assert_eq(x, y) assert_op(x, ==, y)
#define assert_ne(x, y) assert_op(x, !=, y)
#define assert_lt(x, y) assert_op(x, <, y)
#define assert_le(x, y) assert_op(x, <=, y)
#define assert_gt(x, y) assert_op(x, >, y)
#define assert_ge(x, y) assert_op(x, >=, y)

template <typename T>
void __attribute__((noinline, noreturn, cold))
assert_op_fail(const char* file, int line, const char* msg,
               const T& x, const char* op, const T& y) {
    char fmt[48];
    snprintf(fmt, sizeof(fmt), "%%s:%%d: expected %%%s %s %%%s\n",
             printfmt<T>::spec, op, printfmt<T>::spec);
    error_printf(CPOS(22, 0), COLOR_ERROR, fmt, file, line, x, y);
    assert_fail(file, line, msg);
}


// assert_memeq(x, y, sz)
//    If `memcmp(x, y, sz) != 0`, print a message and fail.
#define assert_memeq(x, y, sz)    do {                                  \
        auto __x = (x); auto __y = (y); size_t __sz = (sz);             \
        if (memcmp(__x, __y, __sz) != 0) {                              \
            assert_memeq_fail(__FILE__, __LINE__, "memcmp(" #x ", " #y ", " #sz ") == 0", __x, __y, __sz); \
        }                                                               \
    } while (0)
void __attribute__((noinline, noreturn, cold))
assert_memeq_fail(const char* file, int line, const char* msg,
                  const char* x, const char* y, size_t sz);


// panic(format, ...)
//    Print the message determined by `format` and fail.
void __attribute__((noinline, noreturn, cold))
panic(const char* format, ...);

#endif /* !WEENSYOS_LIB_H */
