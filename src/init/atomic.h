/*! \defgroup _atomic Атомарные операции

  атомарные операции для Cortex-M3 и Intel Core

  \see [ARM IHI 0053C] ARM® C Language Extensions, Release 2.0
   Встроенные инструкции __builtin_ работают на всех платформах Intel, для тех платформ,
   где не определены может использоваться внешняя библиотека
   вызов подменяется на  __sync_bool_compare_and_swap_4

   Для ARMv7 надо переписать с использованием инструкций эксклюзивного доступа


	\see ARM Cortex™-M Programming Guide to Memory Barrier Instructions Application Note 321

 uint32_t __swp(uint32_t x, volatile uint32_t *p) {
    uint32_t v;
 // use LDREX/STREX intrinsics not specified by ACLE
    do v = __ldrex(p); while (__strex(x, p));
    return v;
 }

or alternatively,

 uint32_t __swp(uint32_t x, uint32_t *p) {
 uint32_t v;
 // use IA-64/GCC atomic builtins
 do v = *p; while (!__sync_bool_compare_and_swap(p, v, x));
 return v;
 }

	\todo ISO/IEC 9899:2011 определяет <stdatomic.h> if __STDC_VERSION__ > 201112L
	\todo GCC Built-in

 \see https://www.iso.org/obp/ui/#iso:std:iso-iec:9899:ed-3:v1:en

    \{
*/
#ifndef ATOMIC_H
#define ATOMIC_H
// атомарные операции производим с целым типом
typedef volatile int atomic_t;
#define ACCESS_ONCE(x) (*(volatile typeof(x) *)&(x))
#define compiler_barrier() asm volatile("": : :"memory")

#if (defined(__arm__) && __ARM_ARCH >= 6)
#include <arm_acle.h>
//#include "board.h"
#if (defined(__CORTEX_M) && __CORTEX_M == 0)


static inline int atomic_int_get(volatile int *ptr)
{
    __sync_synchronize();
	return *ptr;
}
static inline void* atomic_pointer_get(volatile void **ptr)
{
    __sync_synchronize();
	return (void*)(*ptr);
}

static inline int atomic_int_compare_and_exchange(volatile int *ptr, int oldval, int newval)
{
	int result;
	__disable_irq();
	result = (*ptr == oldval);
	if (result) {
        *ptr = newval;
        __DSB();
	}
	__enable_irq();
	return result;
}
static inline int atomic_pointer_compare_and_exchange(volatile void **ptr, void *oldval, void *newval)
{
	int result;
	__disable_irq();
	result = (*ptr == oldval);
	if (result) {
        *ptr = newval;
        __DSB();
	}
	__enable_irq();
	return result;
}

#else
/*! \brief Очищает привязку к ячейке памяти выполненную командой LDREX (atomic_int_get)
*/
__attribute__(( always_inline ))
static inline void atomic_free()
{
	__asm volatile ("clrex" ::: "memory");
}
/*! \brief атомарно читает переменную по заданному указателю

    Функцию следует использовать в сочетании с функцией \ref atomic_int_compare_and_exchange

    Вместе они образуют пару LDREX/STREX
    \param ptr указатель на переменную.
    \return значение переменной
 */
__attribute__(( always_inline ))
static inline int atomic_int_get(volatile int *ptr)
{
	int result;
	__asm volatile ("ldrex %0, %1" : "=r" (result) : "Q" (*ptr) ); // \see CMSIS/core_cmIntr.h
	return result;
}
/*! \brief атомарно читает переменную типа указатель по заданному указателю

    Функцию следует использовать в сочетании с функцией \ref atomic_int_compare_and_exchange

    Вместе они образуют пару LDREX/STREX
    \param ptr указатель на переменную.
    \return значение переменной
*/
__attribute__(( always_inline ))
static inline void* atomic_pointer_get(volatile void **ptr)
{
	void* result;
	__asm volatile ("ldrex %0, %1" : "=r" (result) : "Q" (*ptr) ); // \see CMSIS/core_cmIntr.h
	return (void*)result;
}
/*!	\brief производит сравнение и замену. Если значение переменной по указанному адресу изменилось,
    то операция замены не выполняется.

    \param [in] ptr - указатель на атомарную переменную
    \param [in] oldval - старое значение переменной
    \param [in] newval - новое значение переменной
	\return TRUE если операция сохранения прошла успешно, обмен произведен
*/
__attribute__(( always_inline ))
static inline int atomic_int_compare_and_exchange(volatile int *ptr, int oldval, int newval)
{
	int result;
	__asm volatile ("strex %0, %2, %1" : "=&r" (result), "=Q" (*ptr) : "r" (newval) );
//	__DMB();
//	return !result;
	return result==0;
}
/*!	\brief производит сравнение и замену атомарного указателя. Если значение переменной по указанному адресу изменилось,
    то операция замены не выполняется.

    \param [in] ptr - указатель на атомарную переменную
    \param [in] oldval - старое значение переменной
    \param [in] newval - новое значение переменной
	\return TRUE если операция сохранения прошла успешно, обмен произведен
*/
__attribute__(( always_inline ))
static inline int atomic_pointer_compare_and_exchange(volatile void *ptr, void* oldval, void* newval)
{
	int result;
	__asm volatile ("strex %0, %2, %1" : "=&r" (result), "=Q" (*(int*)ptr) : "r" (newval) );
//	__DMB();
	return result==0;
}
#endif
static inline void __DMB() { __asm__ __volatile__("dmb ishst" ::: "memory"); }
#define atomic_mb __DMB//__DSB
#if 0
static inline uint32_t __attribute__((always_inline, nodebug))
__swp(uint32_t x, volatile uint32_t *p) {
    uint32_t v;
    do v = __builtin_arm_ldrex(p); while (__builtin_arm_strex(x, p));
    return v;
}
#endif
#else // для платформы Intel

#define atomic_mb __sync_synchronize
static inline void atomic_free()
{
}
static inline int atomic_int_get(volatile int *v)
{
    __sync_synchronize();// см исходники Glib
    return (*v);
}
static inline void* atomic_pointer_get(volatile void **v)
{
    __sync_synchronize();// см исходники Glib
    return (void*)*v;
}
static inline int atomic_int_compare_and_exchange(volatile int *ptr, int oldval, int newval)
{
//    return __atomic_compare_exchange_4(ptr, &oldval, newval, 0, __ATOMIC_RELAXED, __ATOMIC_RELAXED);
	return __sync_bool_compare_and_swap(ptr, oldval, newval);// returns true if the comparison is successful and newval was written
}
static inline int atomic_pointer_compare_and_exchange(volatile void **ptr, void* oldval, void* newval)
{
//    return __atomic_compare_exchange_{4,8}(ptr, &oldval, newval, 0, __ATOMIC_RELAXED, __ATOMIC_RELAXED);
	return __sync_bool_compare_and_swap(ptr, oldval, newval);// returns true if the comparison is successful and newval was written
}

#endif

#if defined(__STDC_VERSION__) && (__STDC_VERSION__>=201112L) && !defined(__STDC_NO_ATOMICS__)
//#warning "Атомарные типы определены в <stdatomic.h> version="
#include <stdatomic.h>

#define atomic_pointer_exchange(ptr, val) atomic_exchange(ptr, val)
#define atomic_pointer_store(ptr, val) atomic_store(ptr, val)
#else
/*! \brief присвоить значение переменной
    \param [in] ptr указатель на атомарную переменную
    \param [in] newval значение переменной
    \return значение переменной до выполнения операции

	\todo переиментовать в _exchange
 */
static inline int atomic_exchange(volatile int* ptr, int newval)
{
	int value;
	do {
		value = atomic_int_get(ptr);
	} while(!atomic_int_compare_and_exchange(ptr,value,newval));
	return value;
}
/*!	\brief замена атомарной переменной

    используется для замены указателей в списках
    \param [in] atomic - атомарная переменная
    \param [in] newptr - новое значение указателя
    \return значение переменной до выполнения операции
 */
static inline void* atomic_pointer_exchange(volatile void** atomic, void* newptr)
{
	void* oldptr;
	do {
		oldptr = atomic_pointer_get(atomic);
	} while(!atomic_pointer_compare_and_exchange(atomic,oldptr,newptr));
	return oldptr;
}
static inline void atomic_pointer_store(volatile void** atomic, void* newptr)
{
	void* oldptr;
	do {
		oldptr = atomic_pointer_get(atomic);
	} while(!atomic_pointer_compare_and_exchange(atomic,oldptr,newptr));
}

/*! \brief атомарно добавить число к переменной
    \param [in] ptr указатель на атомарную переменную
    \param [in] val довесок
    \return значение переменной до выполнения операции
 */
static inline int atomic_fetch_add(volatile int* ptr, int operand)
{
	int value;
	do {
		value = atomic_int_get(ptr);
	} while(!atomic_int_compare_and_exchange(ptr,value,value+operand));
	return value;
}
/*! \brief атомарно вычесть операнд
	\note Oперации atomic_fetch_add/_sub могут быть использованы для счетчика указателей на объект и в механизме блокировок
	\see [C11] stdatomic.h
	*/
static inline int atomic_fetch_sub(volatile int* ptr, int operand)
{
	int value;
	do {
		value = atomic_int_get(ptr);
	} while(!atomic_int_compare_and_exchange(ptr,value,value-operand));
	return value;
}

/*! \brief атомарно установить биты по маске
    \param [in] ptr указатель на атомарную переменную
    \param [in] mask маска, 1 - биты, которые надо установить, 0 - биты без изменения
    \return значение переменной до выполнения операции
 */
static inline  int atomic_fetch_or(volatile int* ptr, unsigned int mask)
{
	int value;
	do {
		value = atomic_int_get(ptr);
	} while(!atomic_int_compare_and_exchange(ptr,value,value|mask));
	return value;
}
/*! \brief атомарно чистить биты по маске
    \param [in] ptr указатель на атомарную переменную
    \param [in] mask маска, 1 - биты, которые надо оставить без изменения, 0 - биты надо очистить
    \return значение переменной до выполнения операции
 */
static inline int atomic_fetch_and(volatile  int* ptr, unsigned int mask)
{
	int value;
	do {
		value = atomic_int_get(ptr);
	} while(!atomic_int_compare_and_exchange(ptr,value,value & mask));
	return value;
}

static inline int atomic_fetch_xor(volatile  int* ptr, unsigned int mask)
{
	int value;
	do {
		value = atomic_int_get(ptr);
	} while(!atomic_int_compare_and_exchange(ptr,value,value ^ mask));
	return value;
}
#endif


/*! \} */
#endif// ATOMIC_H
