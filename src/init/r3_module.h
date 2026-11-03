/*! \file r3_module.h — optional applet registration without touching the monolith table
 *
 *  Two styles (pick one per build):
 *
 *  1) Linker section (no malloc, no ctor order issues):
 *       MODULE("bmu", applet_bmu);
 *     Linker emits __start_r3_mods … __stop_r3_mods (GNU ld).
 *     Dispatch: linear scan (N ~ 30) or qsort once at start.
 *
 *  2) Constructor list (like hmac digest_register):
 *       MODULE_CTOR("bmu", applet_bmu);
 *
 *  Standalone tool pattern (elf / bmu):
 *       int applet_foo(int argc, char **argv) { … }
 *       __attribute__((weak)) int main(int argc, char **argv) {
 *           return applet_foo(argc, argv);
 *       }
 *  When linked into netd, weak main is overridden by netd's main.
 */
#ifndef R3_MODULE_H
#define R3_MODULE_H

#include <stddef.h>

typedef int (*r3_applet_fn)(int argc, char **argv);

typedef struct r3_module {
	const char   *name;	/* multi-call name, e.g. "bmu" */
	r3_applet_fn  fn;
	int n_options;
	const char**options;
	const char* title;
	const char* usage;
} r3_module_t;
int r3_getopt(int argc, char **argv, char **opt_val, long *opt_mask, 
		const struct r3_module* modinfo);
/* ---- 1) section registration ----
 * Place one static descriptor per translation unit.
 * Section name must be a C identifier for __start_/__stop_ symbols.
 */
#define MODULE(name_str, func) \
	static const r3_module_t __r3_mod_##func \
		__attribute__((used, section("r3_mods"), aligned(sizeof(void *)))) = \
		{ (name_str), (func), 0, NULL, NULL, NULL }

/* GNU ld provides these when any object contributes to section r3_mods */
extern const r3_module_t __start_r3_mods[] __attribute__((weak));
extern const r3_module_t __stop_r3_mods[]  __attribute__((weak));

static inline const r3_module_t *r3_modules_begin(void)
{
	return &__start_r3_mods[0];
}
static inline const r3_module_t *r3_modules_end(void)
{
	return &__stop_r3_mods[0];
}

/** Linear lookup — enough for dozens of applets; no bsearch, no sort. */
static inline const r3_module_t* r3_module_get(const char *key)
{
	const r3_module_t *a = r3_modules_begin();
	const r3_module_t *b = r3_modules_end();
	if (!a || !b || a >= b)
		return NULL;
	for (; a < b; a++) {
		if (a->name && a->fn) {
			const char *s = a->name, *k = key;
			while (*s && *s == *k) { s++; k++; }
			if (*s == 0 && *k == 0)
				return a;
		}
	}
	return NULL;
}

static inline r3_applet_fn r3_module_find(const char *key) 
{
	const r3_module_t *a = r3_module_get(key);
	return a?a->fn:0;
}

/* ---- 2) constructor registration (optional, needs list head) ----
 * #define R3_MODULE_USE_CTOR 1 before include if preferred.
 */
#ifdef R3_MODULE_USE_CTOR
extern void r3_module_register(const r3_module_t *m);
#define MODULE_CTOR(name_str, func) \
	static const r3_module_t __r3_mod_##func = { (name_str), (func) }; \
	static void __attribute__((constructor)) __r3_modreg_##func(void) { \
		r3_module_register(&__r3_mod_##func); \
	}
#endif

/* Standalone: weak main → applet (netd provides strong main) */
#define MODULE_MAIN(func) \
	__attribute__((weak)) int main(int argc, char **argv) { \
		return (func)(argc, argv); \
	}


/* ---- APPLET + KEYS (user exercise) ---- */
#define APPLET(name, title, usage, ...)                          \
	static const char *keys_##name[] = { __VA_ARGS__ };          \
	int applet_##name(int argc, char **argv);                    \
	static const r3_module_t __r3_mod_##name                     \
		__attribute__((used, section("r3_mods"),                 \
			       aligned(sizeof(void *)))) =                   \
		{ #name, applet_##name,                                  \
		  (int)(sizeof(keys_##name) / sizeof(keys_##name[0])),   \
		  keys_##name, title, usage }
// определения типов для r3_getopt должны быть согласованы с iot objects

/* ====================================================================== */
/* ТИПИЗАЦИЯ R3_GETOPT (БИНАРНЫЕ МАРКЕРЫ, IoT objects, UPDATE 2026-10-08) */
/* ====================================================================== */
#define BOOL   "\x01" /* BACnet Tag 1: Логический вентиль JSON-Schema */
#define UINT   "\x02" /* BACnet Tag 2: Беззнаковое целое число */
#define SINT   "\x03" /* BACnet Tag 3: Знаковое целое число */
#define SDNV   "\x04" /* Базис: Упакованные числа переменной длины [RFC 6256] */
#define REAL   "\x05" /* BACnet Tag 5: Вещественные веса тензоров (Double) */
#define BSTR   "\x06" /* BACnet Tag 6: Физический базис: Байт-строка */
#define TSTR   "\x07" /* BACnet Tag 7: Физический базис: Текст-строка */
#define BITS   "\x08" /* BACnet Tag 8: Побитовое поле флагов состояния */
#define ENUM   "\x09" /* BACnet Tag 9: Перечисление стейт-машины SFC */
#define DATE   "\x0A" /* BACnet Tag 10: Календарный массив (UINT + ARRAY) */
#define TIME   "\x0B" /* BACnet Tag 11: Временной массив (SINT + ARRAY) */
#define OID_   "\x0C" /* OID: локальный идентификатор объекта RT+(Instance)+ \sa docID */
#define POP_   "\x0D" /* Позиционная команда CHOICE графа [ $1 op $3 ] */
#define PATH   "\x0E" /* Путь к виртуальному / локальному ресурсу контекста MCP */
#define ANY    "\x0F" /* Произвольный элемент данных (Вложенная под-схема JSON) */
#define UNDEF  "\x7F" /* Неинициализированное / отсутствующее поле схемы */


/* Типы байтовых строк _BSTR + IANA TAG модификатор формата */
#define HEX    "\x06" /* Базовое шестнадцатеричное кодирование буфера */
#define BIGNUM "\x16" /* Большие числа криптографии / AI-координат (#6.2) */

/* Типы текстовых строк _TSTR + IANA TAG модификатор формата (На конце 0x7) */
#define UTF8   "\x07" /* UTF-8 текст (национальные кодировки / короткие fauna ID) */
#define UREF   "\x17" /* IANA CBOR Tag 31: uri-reference (JSON-Schema $ref указатель) */
#define URI_   "\x27" /* IANA CBOR Tag 32: Absolute URI (Абсолютный сетевой адрес) */
#define B64URL "\x37" /* IANA CBOR Tag 33: base64url кодированная строка */
#define BASE64 "\x47" /* IANA CBOR Tag 34: Base64-encoded текстовая строка */
#define REXP   "\x57" /* IANA CBOR Tag 35: Регулярное выражение (JSON-Schema pattern) */
#define MIME   "\x67" /* IANA CBOR Tag 36: MIME-тип / Content-Type ресурса MCP */
#define UUID   "\x77" /* IANA CBOR Tag 37: Бинарный UUID сессии ИИ по [RFC 9562] */

/* Числовые типы SDNV с разделителями (Базис SDNV 0x04 + _TYPE_LL маска префикса) */
#define IPv4   "\x14" /* Актуальный глобальный IANA CBOR Tag 52 [RFC9164] */
#define IPv6   "\x24" /* Актуальный глобальный IANA CBOR Tag 54 [RFC9164] */

#endif /* R3_MODULE_H */
