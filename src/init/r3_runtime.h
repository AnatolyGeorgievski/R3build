#ifndef _R3_RUNTIME_H
#define _R3_RUNTIME_H

#include <stdbool.h>
#include <stdint.h>
#include <stdarg.h>
#include <sys/types.h>
#include <stddef.h>
#include <stdio.h>
#undef fgets
#undef getline
#undef getdelim
#undef __getdelim
// переопределяем модули
#define EXPORT_SYMBOL(s)
#define MODULE_DESCRIPTION(...)
#define MODULE_LICENSE(s)
#ifndef __pure
#define __pure __attribute__((pure))
#endif
#ifndef __weak
#define __weak __attribute__((weak))
#endif
#define fallthrough [[fallthrough]]
#ifndef _unlikely
#define _unlikely(x)     __builtin_expect(!!(x), 0)
#endif
#ifndef _likely
#define _likely(x)     __builtin_expect(!!(x), 1)
#endif

// POSIX process
int      sched_yield(void);
pid_t    getpid (void);
pid_t    getppid(void);
pid_t    gettid (void);
#ifdef __linux__
uid_t    getuid (void);
gid_t    getgid (void);
#endif
char *   getenv (const char *name)  __attribute__((nonnull(1)));
// Linux process
int kill(pid_t pid, int sig);
int tkill(int tid, int sig);
int tgkill(int tgid, int tid, int sig);
int raise(int sig);
// #include <sys/sysinfo.h>
struct sysinfo;
int sysinfo(struct sysinfo *info)  __attribute__((nonnull(1)));

void sync(void);
int syncfs(int fd);
int reboot();

// POSIX time
#include <time.h>
time_t   time(time_t *tloc);
int      clock_gettime  (clockid_t clk_id, struct timespec *tp)  __attribute__((nonnull(2)));
int      clock_nanosleep(clockid_t clock_id, int flags, const struct timespec *request, struct timespec *remain);
int      clock_settime  (clockid_t clk_id, const struct timespec *tp)   __attribute__((nonnull(2)));
// Linux /dev/urandom - источник энтропии

#undef basename
#define basename r3_basename  /* Изолируем имя от системной Musl libc */
char *   basename(const char *s)  __attribute__((nonnull(1)));
#ifdef __linux__
#include <sys/random.h>
// GRND_NONBLOCK -- возвращает -1, EAGAIN в errno если источник не готов
ssize_t  getrandom(void *buf, size_t buflen, unsigned int flags)  __attribute__((nonnull(1)));
#endif
#ifdef getdents64
  #undef getdents64
#endif
#ifdef getdents
  #undef getdents
#endif
// Ring-3 directory
// структура директории R3-dirent
ssize_t  getdents64(int fildes, void *buf, size_t nbyte)  __attribute__((nonnull(2)));
typedef struct _r3_dirent dirent_t;
struct _r3_dirent {
    uint64_t       d_ino;    /* 64-bit inode number */
    uint64_t       d_off;    /* Not an offset; see getdents() */
    unsigned short d_reclen; /* Size of this dirent */
    unsigned char  d_type;   /* File type */
    char           d_name[]; /* Filename (null-terminated) */
};
/*! \brief one page of getdents64; optional glob filter; callback, no heap
 *  @return entries passed to fn this page, or -1 on error
 *  @param next_cursor  set to -1 on EOF, else cursor+1
 *  fn returns 0 to continue, !=0 to stop page early
 */
typedef int (*dirent_fn)(const dirent_t *d, void *user_data);
int dirent_page(int fd, void *buf, size_t buf_size,
                const char *pattern, size_t pat_len,
                dirent_fn fn, void *user_data,
                int cursor, int *next_cursor);

// POSIX/Linux memory manager -- постраничное управление памятью
#define PAGE_SIZE 0x1000
#ifndef loff_t
typedef int64_t loff_t;
#endif
ssize_t copy_file_range(int fd_in, loff_t *off_in, int fd_out, loff_t *off_out, size_t len, unsigned int flags);

// Ring-3 helpers -- компактные утилиты не должны тащить за собой библиотеку libc
/* преобразование чисел в формат float32 дает распределение [0,1) */
static inline float _u64_float(uint64_t x) {
	return ((uint32_t)((x>>32) ^ (x&0xFFFFFFFFU)) >> 8) * 0x1.0p-24f;
}
static inline float _bf16_to_fp32(uint16_t bits) {
    union { uint32_t u; float f; } v;
    // Старшие 16 бит float32 полностью соответствуют битовой сетке BF16
    // Младшие 16 бит мантиссы просто зануляются (lossless по весам экспоненты)
    v.u = (uint32_t)bits << 16;
    return v.f;
}
/**
 * @brief Конвертация FP16 (E5M10) во Float32 (E8M23) строго на битовых масках.
 * @note Финитная логика: субнормали принудительно гасятся в ноль для скорости.
 */
static inline float _fp16_to_fp32(uint16_t bits) {
    union { uint32_t u; float f; } v;
    uint32_t sign = (bits & 0x8000u) << 16;
    uint32_t exp  = (bits & 0x7C00u) >> 10;
    uint32_t mant = (bits & 0x03FFu);
    if (__builtin_expect(exp == 0, 0)) {
        // Финитное гашение subnormal/zero в чистый знаковый ноль
        v.u = sign;
        return v.f;
    }
    if (__builtin_expect(exp == 0x1F, 0)) {
        // Насыщение NaN / Infinity
        v.u = sign | 0x7F800000u | (mant << 13);
        return v.f;
    }
    // Смещение экспоненты: FP16 bias (15) -> FP32 bias (127)
    uint32_t new_exp = exp + (127 - 15);
    v.u = sign | (new_exp << 23) | (mant << 13);
    return v.f;
}
// 🧬 Замена <math.h>
static inline double _ldexp(double val, int exp) {
    return __builtin_ldexp(val, exp);
}
static inline float _ldexpf(float val, int exp) {
    return __builtin_ldexpf(val, exp);
}
static inline double _frexp(double val, int * exp) {
    return __builtin_frexp(val, exp);
}
static inline float _frexpf(float val, int * exp) {
    return __builtin_frexpf(val, exp);
}
// 🧬 Замена <ctype.h> без таблиц
static inline int _islower(int c) {
	return (unsigned)(c - 'a')<=('z'-'a');
}
static inline int _isupper(int c) {
	return (unsigned)(c - 'A')<=('Z'-'A');
}
static inline int _toupper(int c) {
	return _islower(c)? c&~0x20: c;
}
static inline int _tolower(int c) {
	return _isupper(c)? c| 0x20: c;
}
static inline int _isdigit_base(int c, unsigned base) {
	return (unsigned)(c - '0')<base;
}
static inline int _isdigit(int c) {
    return (unsigned)(c - '0') <= ('9' - '0');
}
static inline int _isoctal(int c) {
    return (unsigned)(c - '0') <= ('7' - '0');
}
static inline int _isxdigit(int c) {
    return _isdigit(c) || 
           (unsigned)((c | 0x20) - 'a') <= ('f' - 'a'); // трюк (c|0x20) объединяет регистры a-f и A-F
}
static inline int _isalpha(int c) {
    return (unsigned)((c | 0x20) - 'a') <= ('z' - 'a'); // маска |0x20 переводит любой регистр в нижний
}
// Стандарт POSIX определяет их как: ' ', '\t', '\n', '\v', '\f', '\r'
static inline int _isspace(int c) {
    return (c == ' ') || ((unsigned)(c - '\t') <= ('\r' - '\t'));
}
static inline int _iscntrl(int c) {
    return (unsigned)c < 0x20 || (c == 0x7F);
}
static inline int _isprint(int c) {
    return (unsigned)(c - 0x20) <= (0x7E - 0x20);
}
static inline int _isgraph(int c) {
    return (unsigned)(c - 0x21) <= (0x7E - 0x21);
}
static inline unsigned long _strtoul(const char *s, char **e, unsigned base) {
    unsigned long v = 0;
    while (_isdigit_base(*s, base))
        v = v * base + (*s++ - '0');
	if (e) *e = (char*)s;
    return v;
}
static inline int _strcmp(const char *s1, const char *s2) {
    while (*s1 && (*s1 == *s2)) { s1++; s2++; }
    return *(const unsigned char*)s1 - *(const unsigned char*)s2;
}
static inline int _strncmp(const char *_l, const char *_r, size_t n) {
    // Каст к unsigned char убирает накладные расходы на знаковое расширение (MOVSX/SXTB)
    const unsigned char *l = (const unsigned char *)_l;
    const unsigned char *r = (const unsigned char *)_r;
    if (!n--) return 0;
    while (*l && *l == *r && n--) {
        l++; r++;
    }
    return *l - *r;
}

static inline size_t _strlen(const char *str) {
    register const char* s = str;
    while(*s)
        s ++;
    return s - str;
}
// добавить функцию _strnlen(const char*, size_t)

// определения из r3_utf8 
// длина последовательности вычисляется по первому символу
static inline int _utf8_len(uint32_t ch) {
    return ch>=0x80?__builtin_clz(~(ch<<24)): 1;
}
static inline const char* _utf8_next_char(const char* s) {
	// uint32_t ch = *(uint8_t*)s;
	return s + _utf8_len(*s);//(ch >= 0x80?_utf8_len(ch):1);
}
// добавить функцию _utf8_strnlen(const char*, size_t)
static inline long _utf8_strlen(const char* str) {
    register const char* s = str;
    while(*s)
        s = _utf8_next_char(s);
    return s - str;
}
static inline long _utf8_strnlen(const char* str, size_t max_size) {
    register const char* s = str;
    while(*s && s-str<max_size)
        s = _utf8_next_char(s);
    return s - str;
}
/* Двухбайтовый маркер в регистре. Канон: net_template.c ({{ {% {# ${).
 * Два литерала в одном месте (oid `${`) — s[0]=='$' && s[1]=='{', не это. */
static inline int   _swar_cmp(const char *s, uint32_t tpl) {
    const uint16_t mask = 0xFFFF;
    return ((*(uint16_t*)s & mask) - (uint16_t)tpl);
}
// выделение правой SWAR left bracket границы
static inline char* _swar_rchr(const char *s, uint32_t tpl) {
    while (*s) {// s = _swar_strrchr(s ,W_JINJA_COMMENT_CLOSE, W_MASK);
        if (_swar_cmp(s, tpl) == 0) break;
        s = _utf8_next_char(s);
    }
    return (char*)s;
}
/* Шаблон с *: gpio2 → "gpio*" + index. Не rt@ins (то object_oid_from_len). */
static inline int  _cname(char* cname, const char* name, char ** end){
    int index = 0;
    const char* s = name;
    while (*s!='\0' && !_isdigit(*s)) *cname++ = *s++;
    if (_isdigit(*s)) *cname++ = '*'; 
    while (_isdigit(*s)) index = index*10+(*s++ -'0');
    while (*s!='\0') *cname++ = *s++; // до конца строки
    *cname++ = '\0';
    if (end) *end = cname;
    return index;
}

#if defined(__BYTE_ORDER__) && (__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__)
static inline uint64_t _ntohll(uint64_t a){ return __builtin_bswap64(a); }
static inline uint64_t _htonll(uint64_t a){ return __builtin_bswap64(a); }
static inline uint32_t _ntohl (uint32_t a){ return __builtin_bswap32(a); }
static inline uint32_t _htonl (uint32_t a){ return __builtin_bswap32(a); }
static inline uint16_t _ntohs (uint16_t a){ return __builtin_bswap16(a); }
static inline uint16_t _htons (uint16_t a){ return __builtin_bswap16(a); }
#else
static inline uint32_t _htonl (uint32_t a){ return (a); }
static inline uint32_t _ntohl (uint32_t a){ return (a); }
static inline uint16_t _htons (uint16_t a){ return (a); }
static inline uint16_t _ntohs (uint16_t a){ return (a); }
#endif

// Ring-3 atomics -- основа lock-free mutex, semaphores
#include "atomic.h"
typedef struct _List List_t;
struct _List {
    List_t* next;
};
/*! \brief атомарно пихает в стек */
static inline void atomic_list_push(volatile void** top, List_t* elem)
{
	do {
		elem->next = atomic_pointer_get(top);
		atomic_mb();
	} while(!atomic_pointer_compare_and_exchange(top, elem->next, elem));
}
/*! \brief атомарно выталкивает со стека */
static inline void* atomic_list_pop(volatile void** top)
{
	List_t* ref;
	do {
		ref = atomic_pointer_get(top);
		if(ref==NULL){
			atomic_free();
			break;
		}
	} while(!atomic_pointer_compare_and_exchange(top, ref, ref->next));
	return ref;
}
// представление времени в виде целой и дробной части uint64_t используется в osTimestamp()
typedef uint64_t time64_t;
static inline uint64_t timespec_to_time64(const struct timespec *ts){
    return ((uint64_t)ts->tv_sec<<32) + (ts->tv_nsec<<2);
}
static inline void timespec_from_time64(struct timespec *ts, time64_t t64){
    ts->tv_sec  = t64>>32;
    ts->tv_nsec = ((uint32_t)t64)>>2;
}
static inline time64_t time64_bcd(time64_t t64){ // коррекция времени
    if ((uint32_t)(t64 + 0x1194D800UL) < (uint32_t)t64)
        t64 = (t64 + 0x1194D800UL);
    return t64;
}
/*! \defgroup _semaphore_base Базовое определение семафоров

    Семафор — это объект, с которым можно выполнить три операции:
 - init(n): счётчик := n
 - enter(): ждать пока счётчик станет больше 0; после этого уменьшить счётчик на единицу.
 - leave(): увеличить счётчик на единицу.

    семафоры могут использоваться в ядре и в прерываниях. На базовом определении семафоров строятся \ref _memory_pool "osPool выделение памяти из таблицы", \ref _mutex "Мьютексы" и \ref _semaphore "Семафоры"

	\see The Open Group Base Specifications Issue 7, 2018 edition
	\see IEEE Std 1003.1-2017 (Revision of IEEE Std 1003.1-2008)
https://pubs.opengroup.org/onlinepubs/9699919799/
    \{
*/

#define SEMAPHORE_INIT(n) (n)
/*! \brief инициализация семафора

 */
static inline void semaphore_init(volatile int * ptr, int count)
{
    *ptr = count;
//    atomic_mb();
	//__DMB();
}
/*! \brief вход в критическую область

    Функция НЕ блокирует исполнение, только убавляет счетчик входов на единицу.

    \return счетчик семафора до вычитания, 0 - семафор занят.
 */
static inline int semaphore_enter(volatile int * ptr)
{
    register int count;
    do {
        count = atomic_int_get(ptr);
        if((count==0)) {
			atomic_free(); 
			// __ldrex и __strex идут в паре, надо выполнить операцию __clrex, но кажется не обязательно
			break;
		}
    } while(!atomic_int_compare_and_exchange(ptr, count, count-1));
    //atomic_mb();
	//__DMB();
    return count;
}
/*! \brief выход из критической области
 */
static inline int semaphore_leave(volatile int * ptr)
{
//	__DMB();
    return atomic_fetch_add(ptr, 1);
    //atomic_mb();// сбрасываем ?
}
/*! \} */
/**
 * @brief Аллокация выровненного буфера из пула быстрых атомарных слайсов.
 * @note Гарантирует выравнивание кратно странице памяти хоста (4 KB).
 */
__attribute__((malloc, alloc_size(1), warn_unused_result))
void *g_slice_alloc(size_t size);

/**
 * @brief Безвозвратное возвращение слайса памяти в атомарный стек пустых блоков.
 */
void g_slice_free1(size_t size, void *mem);

#endif//_R3_RUNTIME_H
