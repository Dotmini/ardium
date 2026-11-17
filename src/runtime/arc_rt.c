/* ==========================================================================
 * Arc language runtime
 *
 * This file is embedded into the `arc` compiler and prepended to every
 * generated C translation unit. It provides:
 *   - a precise mark & sweep garbage collector (shadow-stack roots)
 *   - strings, dynamic arrays, insertion-ordered hash maps
 *   - value formatting (print / str() / f-strings)
 *   - panics with source locations, stack-overflow detection
 *   - a small standard library (I/O, time, random, parsing)
 * ========================================================================== */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include <stdarg.h>
#include <math.h>
#include <setjmp.h>
#include <signal.h>
#include <time.h>
#include <unistd.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <dirent.h>
#include <errno.h>

#define ARC_UNLIKELY(x) __builtin_expect(!!(x), 0)
#define ARC_LIKELY(x) __builtin_expect(!!(x), 1)
#define ARC_NORET __attribute__((noreturn))
#define ARC_COLD __attribute__((cold, noinline))
#define ARC_UNUSED __attribute__((unused))

/* ---------------------------------------------------------------- types -- */

typedef struct { int32_t file, line, col; } ArcLoc;
#define ARC_LOC(f, l, c) ((ArcLoc){(f), (l), (c)})
static const char *const *arc_file_names = NULL;

enum {
    ARC_K_INT = 1, ARC_K_FLOAT, ARC_K_BOOL, ARC_K_STR,
    ARC_K_ARR, ARC_K_MAP, ARC_K_STRUCT, ARC_K_OPT,
    ARC_K_ENUM,  /* object: tag + flattened payload fields */
    ARC_K_RES,   /* `!T` value: heap repr (pointer to T or ArcErr) or {ArcErr *err; T val} */
    ARC_K_FN,    /* closure pointer */
    ARC_K_ERR    /* ArcErr object */
};

typedef struct ArcTD ArcTD;
typedef struct { const char *name; size_t offset; const ArcTD *td; } ArcField;

/* Type descriptor: drives GC scanning, printing, hashing and comparison. */
struct ArcTD {
    int kind;
    int is_heap;            /* value is a (nullable) pointer to a GC object */
    size_t size;            /* size of the value when stored inline */
    const ArcTD *elem;      /* array element / map value / optional payload */
    const ArcTD *key;       /* map key */
    const char *name;       /* struct name */
    int nfields;
    const ArcField *fields;
    size_t obj_size;        /* struct object allocation size */
    size_t opt_off;         /* payload offset inside an inline optional / result */
    int nvariants;          /* enums: variant names and their payload fields */
    const char *const *vnames;
    const int *vfirst;
    const int *vcount;
};

typedef struct ArcObj {
    struct ArcObj *next;
    const ArcTD *td;
    uint32_t flags;
    uint32_t _pad;
} ArcObj;
#define ARC_F_MARK 1u
#define ARC_F_STATIC 2u
#define ARC_F_FREE 4u
#define ARC_F_LARGE 8u

typedef struct { ArcObj h; int64_t len; const char *data; } ArcStr;
typedef struct { ArcObj h; int64_t len, cap; size_t esize; char *data; } ArcArr;
typedef struct {
    ArcObj h;
    int64_t len;            /* live entries */
    int64_t nent, capent;   /* used / allocated entry slots */
    size_t ksize, vsize, koff, voff, esize;
    char *ent;
    int64_t *idx;           /* open-addressing index: -1 empty, -2 deleted */
    int64_t icap;
} ArcMap;

typedef struct { bool has; int64_t val; } ArcOptInt;
typedef struct { bool has; double val; } ArcOptFloat;
typedef struct { bool has; bool val; } ArcOptBool;

/* Shadow-stack frame: every generated function that holds GC references
 * registers the addresses of those locals here. */
typedef struct ArcFrame { struct ArcFrame *prev; int64_t n; void ***slots; } ArcFrame;

/* an error value carried by `!T` */
typedef struct { ArcObj h; ArcStr *msg; } ArcErr;
/* a closure: generated code extends this with captured values */
typedef struct ArcClo { ArcObj h; void *fn; } ArcClo;
/* enum objects: header, tag, then payload fields */
typedef struct { ArcObj h; int64_t tag; } ArcEnumHdr;

/* Primitive descriptors */
static const ArcTD arc_td_int = {ARC_K_INT, 0, sizeof(int64_t), 0, 0, 0, 0, 0, 0, 0};
static const ArcTD arc_td_float = {ARC_K_FLOAT, 0, sizeof(double), 0, 0, 0, 0, 0, 0, 0};
static const ArcTD arc_td_bool = {ARC_K_BOOL, 0, sizeof(bool), 0, 0, 0, 0, 0, 0, 0};
static const ArcTD arc_td_str = {ARC_K_STR, 1, sizeof(void *), 0, 0, 0, 0, 0, 0, 0};
static const ArcTD arc_td_opt_int = {ARC_K_OPT, 0, sizeof(ArcOptInt), &arc_td_int, 0, 0, 0, 0, 0, offsetof(ArcOptInt, val)};
static const ArcTD arc_td_opt_float = {ARC_K_OPT, 0, sizeof(ArcOptFloat), &arc_td_float, 0, 0, 0, 0, 0, offsetof(ArcOptFloat, val)};
static const ArcTD arc_td_opt_bool = {ARC_K_OPT, 0, sizeof(ArcOptBool), &arc_td_bool, 0, 0, 0, 0, 0, offsetof(ArcOptBool, val)};
static const ArcTD arc_td_opt_str = {ARC_K_OPT, 1, sizeof(void *), &arc_td_str, 0, 0, 0, 0, 0, 0};
static const ArcTD arc_td_arr_int = {ARC_K_ARR, 1, sizeof(void *), &arc_td_int, 0, 0, 0, 0, 0, 0};
static const ArcTD arc_td_arr_float = {ARC_K_ARR, 1, sizeof(void *), &arc_td_float, 0, 0, 0, 0, 0, 0};
static const ArcTD arc_td_arr_bool = {ARC_K_ARR, 1, sizeof(void *), &arc_td_bool, 0, 0, 0, 0, 0, 0};
static const ArcTD arc_td_arr_str = {ARC_K_ARR, 1, sizeof(void *), &arc_td_str, 0, 0, 0, 0, 0, 0};
static const ArcTD arc_td_err = {ARC_K_ERR, 1, sizeof(void *), 0, 0, "error", 0, 0, sizeof(ArcErr), 0};
static const ArcTD arc_td_fn = {ARC_K_FN, 1, sizeof(void *), 0, 0, 0, 0, 0, 0, 0};

/* ---------------------------------------------------------------- state -- */

static ArcFrame *arc_ftop = NULL;
static ArcObj *arc_heap_list = NULL;
static size_t arc_alloc_bytes = 0;
static size_t arc_live_bytes = 0;
static size_t arc_gc_threshold = (size_t)8 << 20;
static int arc_gc_inhibit = 0;
static int arc_gc_stress = 0;
static size_t arc_gc_count = 0;
static void ***arc_groots = NULL;
static size_t arc_ngroots = 0, arc_capgroots = 0;
static ArcArr *arc_args_arr = NULL;
static char *arc_stack_base = NULL;
static size_t arc_stack_limit = 0;

/* test-runner support */
static jmp_buf *arc_test_jmp = NULL;
static char arc_test_msg[2048];

/* ---------------------------------------------------------------- panic -- */

static void arc_flush_all(void) { fflush(stdout); fflush(stderr); }

static void arc_write_loc(FILE *f, ArcLoc loc) {
    if (loc.line > 0 && arc_file_names)
        fprintf(f, "  --> %s:%d:%d\n", arc_file_names[loc.file], loc.line, loc.col);
}

ARC_COLD ARC_NORET static void arc_panic_v(ArcLoc loc, const char *fmt, va_list ap) {
    if (arc_test_jmp) {
        int n = vsnprintf(arc_test_msg, sizeof arc_test_msg, fmt, ap);
        if (n < 0) n = 0;
        if ((size_t)n < sizeof arc_test_msg && loc.line > 0 && arc_file_names)
            snprintf(arc_test_msg + n, sizeof arc_test_msg - (size_t)n, "\n  --> %s:%d:%d",
                     arc_file_names[loc.file], loc.line, loc.col);
        longjmp(*arc_test_jmp, 1);
    }
    fflush(stdout);
    fprintf(stderr, "panic: ");
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");
    arc_write_loc(stderr, loc);
    fflush(stderr);
    exit(101);
}

ARC_COLD ARC_NORET static void arc_panic_at(ArcLoc loc, const char *fmt, ...)
    __attribute__((format(printf, 2, 3)));
ARC_COLD ARC_NORET static void arc_panic_at(ArcLoc loc, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    arc_panic_v(loc, fmt, ap);
}

ARC_COLD ARC_NORET static void arc_oom(void) {
    fflush(stdout);
    fprintf(stderr, "fatal: out of memory\n");
    exit(134);
}

ARC_COLD ARC_NORET static void arc_capacity_overflow(void) {
    arc_panic_at((ArcLoc){0, 0, 0}, "capacity overflow: the requested size does not fit in memory");
}
/* a * b for allocation sizes; panics instead of silently wrapping */
static inline size_t arc_mul_size(size_t a, size_t b) {
    size_t r;
    if (ARC_UNLIKELY(__builtin_mul_overflow(a, b, &r))) arc_capacity_overflow();
    return r;
}
static void *arc_xmalloc(size_t n) { void *p = malloc(n ? n : 1); if (!p) arc_oom(); return p; }
static void *arc_xcalloc(size_t n) { void *p = calloc(1, n ? n : 1); if (!p) arc_oom(); return p; }
static void *arc_xrealloc(void *p, size_t n) { void *q = realloc(p, n ? n : 1); if (!q) arc_oom(); return q; }

ARC_NORET static void arc_exit(int64_t code) {
    arc_flush_all();
    exit((int)code);
}

/* ------------------------------------------------------------------- GC -- */

static ArcObj **arc_mstack = NULL;
static size_t arc_mlen = 0, arc_mcap = 0;

static inline void arc_mark(void *p) {
    ArcObj *o = (ArcObj *)p;
    if (!o || (o->flags & (ARC_F_MARK | ARC_F_STATIC))) return;
    o->flags |= ARC_F_MARK;
    if (arc_mlen == arc_mcap) {
        arc_mcap = arc_mcap ? arc_mcap * 2 : 1024;
        arc_mstack = (ArcObj **)arc_xrealloc(arc_mstack, arc_mcap * sizeof(ArcObj *));
    }
    arc_mstack[arc_mlen++] = o;
}

static void arc_mark_value(const void *p, const ArcTD *td) {
    if (td->is_heap) arc_mark(*(void *const *)p);
    else if (td->kind == ARC_K_RES) arc_mark(*(void *const *)p);  /* {err, val}: err is first */
}
static inline int arc_td_has_ptr(const ArcTD *td) { return td->is_heap || td->kind == ARC_K_RES; }

static void arc_scan(ArcObj *o) {
    const ArcTD *td = o->td;
    switch (td->kind) {
    case ARC_K_ARR: {
        ArcArr *a = (ArcArr *)o;
        if (td->elem->is_heap) {
            void **d = (void **)a->data;
            for (int64_t i = 0; i < a->len; i++) arc_mark(d[i]);
        } else if (td->elem->kind == ARC_K_RES) {
            for (int64_t i = 0; i < a->len; i++) arc_mark(*(void **)(a->data + (size_t)i * a->esize));
        }
        break;
    }
    case ARC_K_MAP: {
        ArcMap *m = (ArcMap *)o;
        int kh = td->key->is_heap, vh = arc_td_has_ptr(td->elem);
        if (!kh && !vh) break;
        for (int64_t i = 0; i < m->nent; i++) {
            char *e = m->ent + (size_t)i * m->esize;
            if (!e[8]) continue;
            if (kh) arc_mark(*(void **)(e + m->koff));
            if (vh) arc_mark_value(e + m->voff, td->elem);
        }
        break;
    }
    case ARC_K_STRUCT: case ARC_K_ENUM:
        for (int i = 0; i < td->nfields; i++)
            arc_mark_value((char *)o + td->fields[i].offset, td->fields[i].td);
        break;
    case ARC_K_ERR:
        arc_mark(((ArcErr *)o)->msg);
        break;
    default:
        break;
    }
}

static size_t arc_obj_bytes(ArcObj *o) {
    switch (o->td->kind) {
    case ARC_K_STR: return sizeof(ArcStr) + (size_t)((ArcStr *)o)->len + 1;
    case ARC_K_ARR: return sizeof(ArcArr) + (size_t)((ArcArr *)o)->cap * ((ArcArr *)o)->esize;
    case ARC_K_MAP: {
        ArcMap *m = (ArcMap *)o;
        return sizeof(ArcMap) + (size_t)m->capent * m->esize + (size_t)m->icap * sizeof(int64_t);
    }
    case ARC_K_STRUCT: case ARC_K_ENUM: case ARC_K_ERR: return o->td->obj_size;
    default: return sizeof(ArcObj);
    }
}

/* Release the out-of-line buffers an object owns (array data, map tables). */
static void arc_finalize(ArcObj *o) {
    switch (o->td->kind) {
    case ARC_K_ARR: free(((ArcArr *)o)->data); break;
    case ARC_K_MAP: free(((ArcMap *)o)->ent); free(((ArcMap *)o)->idx); break;
    default: break;
    }
}

/* Small objects (<= 512 bytes) live in 64 KiB pages split into fixed-size
 * slots, one page list per size class; freed slots go on a per-class free
 * list. Larger objects are malloc'd individually and kept on a linked list. */
#define ARC_NCLASS 15
#define ARC_PAGE_SIZE ((size_t)64 << 10)
#define ARC_SMALL_MAX 512
static const uint32_t arc_class_size[ARC_NCLASS] = {32, 48, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320, 384, 448, 512};
static uint8_t arc_class_of[ARC_SMALL_MAX / 16 + 1];

typedef struct ArcPage {
    struct ArcPage *next;
    uint32_t cls, nslots, bump, _pad;
} ArcPage;
#define ARC_PAGE_HDR ((sizeof(ArcPage) + 15) & ~(size_t)15)

static ArcPage *arc_pages[ARC_NCLASS];
static ArcPage *arc_cur_page[ARC_NCLASS];
static ArcObj *arc_free_list[ARC_NCLASS];

static void arc_init_classes(void) {
    int c = 0;
    for (int i = 0; i <= ARC_SMALL_MAX / 16; i++) {
        while (arc_class_size[c] < (uint32_t)i * 16) c++;
        arc_class_of[i] = (uint8_t)c;
    }
}

static inline ArcObj *arc_page_slot(ArcPage *p, uint32_t i) {
    return (ArcObj *)((char *)p + ARC_PAGE_HDR + (size_t)i * arc_class_size[p->cls]);
}

static ArcObj *arc_small_alloc(int cls, size_t size) {
    ArcObj *o = arc_free_list[cls];
    if (o) {
        arc_free_list[cls] = o->next;
        memset(o, 0, size);
        return o;
    }
    ArcPage *p = arc_cur_page[cls];
    if (!p || p->bump == p->nslots) {
        p = (ArcPage *)arc_xcalloc(ARC_PAGE_SIZE);
        p->cls = (uint32_t)cls;
        p->nslots = (uint32_t)((ARC_PAGE_SIZE - ARC_PAGE_HDR) / arc_class_size[cls]);
        p->next = arc_pages[cls];
        arc_pages[cls] = p;
        arc_cur_page[cls] = p;
    }
    return arc_page_slot(p, p->bump++);  /* fresh page memory is already zero */
}

static void arc_gc_collect(void) {
    arc_gc_count++;
    for (ArcFrame *f = arc_ftop; f; f = f->prev)
        for (int64_t i = 0; i < f->n; i++) arc_mark(*f->slots[i]);
    for (size_t i = 0; i < arc_ngroots; i++) arc_mark(*arc_groots[i]);
    while (arc_mlen) arc_scan(arc_mstack[--arc_mlen]);

    size_t live = 0;
    /* sweep small-object pages */
    for (int c = 0; c < ARC_NCLASS; c++) {
        arc_free_list[c] = NULL;
        ArcPage **pp = &arc_pages[c];
        while (*pp) {
            ArcPage *p = *pp;
            uint32_t nlive = 0;
            for (uint32_t i = 0; i < p->bump; i++) {
                ArcObj *o = arc_page_slot(p, i);
                if (o->flags & ARC_F_FREE) continue;
                if (o->flags & ARC_F_MARK) { nlive++; continue; }
                arc_finalize(o);
                o->flags = ARC_F_FREE;
            }
            if (nlive == 0 && p != arc_cur_page[c]) {  /* whole page is garbage: return it */
                *pp = p->next;
                free(p);
                continue;
            }
            for (uint32_t i = 0; i < p->bump; i++) {
                ArcObj *o = arc_page_slot(p, i);
                if (o->flags & ARC_F_FREE) {
                    o->next = arc_free_list[c];
                    arc_free_list[c] = o;
                } else {
                    o->flags &= ~ARC_F_MARK;
                    live += arc_obj_bytes(o);
                }
            }
            pp = &p->next;
        }
    }
    /* sweep large objects */
    ArcObj **lp = &arc_heap_list;
    while (*lp) {
        ArcObj *o = *lp;
        if (o->flags & ARC_F_MARK) {
            o->flags &= ~ARC_F_MARK;
            live += arc_obj_bytes(o);
            lp = &o->next;
        } else {
            *lp = o->next;
            arc_finalize(o);
            free(o);
        }
    }
    arc_live_bytes = live;
    arc_alloc_bytes = 0;
    size_t min_thr = (size_t)8 << 20;
    arc_gc_threshold = live > min_thr ? live : min_thr;
}

static void *arc_alloc(const ArcTD *td, size_t size) {
    if (ARC_UNLIKELY(arc_alloc_bytes >= arc_gc_threshold || arc_gc_stress) && !arc_gc_inhibit)
        arc_gc_collect();
    ArcObj *o;
    if (ARC_LIKELY(size <= ARC_SMALL_MAX)) {
        int cls = arc_class_of[(size + 15) >> 4];
        o = arc_small_alloc(cls, size);
        arc_alloc_bytes += arc_class_size[cls];
    } else {
        o = (ArcObj *)arc_xcalloc(size);
        o->flags = ARC_F_LARGE;
        o->next = arc_heap_list;
        arc_heap_list = o;
        arc_alloc_bytes += size;
    }
    o->td = td;
    return o;
}

static void arc_add_groot(void **p) {
    if (arc_ngroots == arc_capgroots) {
        arc_capgroots = arc_capgroots ? arc_capgroots * 2 : 32;
        arc_groots = (void ***)arc_xrealloc(arc_groots, arc_capgroots * sizeof(void **));
    }
    arc_groots[arc_ngroots++] = p;
}

static void *arc_new_struct(const ArcTD *td) { return arc_alloc(td, td->obj_size); }

/* -------------------------------------------------------------- buffers -- */

typedef struct { char *p; size_t len, cap; char inl[112]; } ArcBuf;
static inline void arc_buf_init(ArcBuf *b) { b->p = b->inl; b->len = 0; b->cap = sizeof b->inl; }

static void arc_buf_grow(ArcBuf *b, size_t extra) {
    size_t nc = b->cap ? b->cap * 2 : 64;
    while (nc < b->len + extra + 1) nc *= 2;
    if (b->p == b->inl) {
        char *np = (char *)arc_xmalloc(nc);
        memcpy(np, b->inl, b->len);
        b->p = np;
    } else {
        b->p = (char *)arc_xrealloc(b->p, nc);
    }
    b->cap = nc;
}
static inline void arc_buf_reserve(ArcBuf *b, size_t extra) {
    if (ARC_UNLIKELY(b->len + extra + 1 > b->cap)) arc_buf_grow(b, extra);
}
static void arc_buf_put(ArcBuf *b, const char *s, size_t n) {
    arc_buf_reserve(b, n);
    memcpy(b->p + b->len, s, n);
    b->len += n;
}
static void arc_buf_cstr(ArcBuf *b, const char *s) { arc_buf_put(b, s, strlen(s)); }
static void arc_buf_putc(ArcBuf *b, char c) { arc_buf_reserve(b, 1); b->p[b->len++] = c; }
static void arc_buf_free(ArcBuf *b) { if (b->p != b->inl) free(b->p); b->p = NULL; b->len = b->cap = 0; }

static void arc_buf_int(ArcBuf *b, int64_t v) {
    char tmp[24];
    int i = 24;
    uint64_t u = v < 0 ? 0 - (uint64_t)v : (uint64_t)v;
    do { tmp[--i] = (char)('0' + u % 10); u /= 10; } while (u);
    if (v < 0) tmp[--i] = '-';
    arc_buf_put(b, tmp + i, (size_t)(24 - i));
}

static void arc_buf_float(ArcBuf *b, double v) {
    char tmp[64];
    if (isnan(v)) { arc_buf_cstr(b, "NaN"); return; }
    if (isinf(v)) { arc_buf_cstr(b, v < 0 ? "-inf" : "inf"); return; }
    if (v == 0) { arc_buf_cstr(b, signbit(v) ? "-0.0" : "0.0"); return; }
    /* shortest round-tripping digits */
    int prec;
    for (prec = 1; prec <= 17; prec++) {
        snprintf(tmp, sizeof tmp, "%.*e", prec - 1, v);
        if (strtod(tmp, NULL) == v) break;
    }
    if (prec > 17) prec = 17;
    char *e = strchr(tmp, 'e');
    int exp10 = e ? atoi(e + 1) : 0;
    if (exp10 < -4 || exp10 >= 16) {
        /* scientific: mantissa without trailing zeros, Python style 1e+16 */
        char mant[40];
        size_t ml = (size_t)(e - tmp);
        memcpy(mant, tmp, ml);
        mant[ml] = 0;
        arc_buf_cstr(b, mant);
        char ex[16];
        snprintf(ex, sizeof ex, "e%c%02d", exp10 < 0 ? '-' : '+', exp10 < 0 ? -exp10 : exp10);
        arc_buf_cstr(b, ex);
        return;
    }
    int decimals = prec - 1 - exp10;
    if (decimals < 1) decimals = 1;
    int n = snprintf(tmp, sizeof tmp, "%.*f", decimals, v);
    /* trim trailing zeros but keep one digit after the point */
    while (n > 0 && tmp[n - 1] == '0' && tmp[n - 2] != '.') n--;
    arc_buf_put(b, tmp, (size_t)n);
}

static void arc_buf_float_prec(ArcBuf *b, double v, int prec) {
    int n = snprintf(NULL, 0, "%.*f", prec, v);
    if (n <= 0) return;
    arc_buf_reserve(b, (size_t)n + 1);
    snprintf(b->p + b->len, (size_t)n + 1, "%.*f", prec, v);
    b->len += (size_t)n;
}

static void arc_buf_bool(ArcBuf *b, bool v) { arc_buf_cstr(b, v ? "true" : "false"); }
static void arc_buf_str(ArcBuf *b, const ArcStr *s) { arc_buf_put(b, s->data, (size_t)s->len); }

static void arc_buf_quoted(ArcBuf *b, const ArcStr *s) {
    arc_buf_putc(b, '"');
    for (int64_t i = 0; i < s->len; i++) {
        unsigned char c = (unsigned char)s->data[i];
        switch (c) {
        case '"': arc_buf_put(b, "\\\"", 2); break;
        case '\\': arc_buf_put(b, "\\\\", 2); break;
        case '\n': arc_buf_put(b, "\\n", 2); break;
        case '\t': arc_buf_put(b, "\\t", 2); break;
        case '\r': arc_buf_put(b, "\\r", 2); break;
        default:
            if (c < 0x20) {
                char tmp[8];
                snprintf(tmp, sizeof tmp, "\\x%02x", c);
                arc_buf_cstr(b, tmp);
            } else {
                arc_buf_putc(b, (char)c);
            }
        }
    }
    arc_buf_putc(b, '"');
}

/* Pad the text appended since `start` to `width` (UTF-8 aware). */
static void arc_buf_align(ArcBuf *b, size_t start, int64_t width, char align, char fill) {
    size_t n = b->len - start;
    int64_t chars = 0;
    for (size_t i = start; i < b->len; i++)
        if (((unsigned char)b->p[i] & 0xC0) != 0x80) chars++;
    if (chars >= width) return;
    size_t pad = (size_t)(width - chars);
    size_t left = 0, right = 0;
    if (align == '<') right = pad;
    else if (align == '^') { left = pad / 2; right = pad - left; }
    else left = pad;
    arc_buf_reserve(b, pad);
    memmove(b->p + start + left, b->p + start, n);
    memset(b->p + start, fill, left);
    memset(b->p + start + left + n, fill, right);
    b->len += pad;
}

static void arc_fmt_value(ArcBuf *b, const void *p, const ArcTD *td, int quoted, int depth);

static const void *arc_fmt_path[32];

/* true if `o` is already being printed further up (a reference cycle) */
static int arc_fmt_enter(ArcBuf *b, const void *o, int depth) {
    for (int i = 0; i < depth && i < 32; i++)
        if (arc_fmt_path[i] == o) { arc_buf_cstr(b, "<cycle>"); return 0; }
    if (depth < 32) arc_fmt_path[depth] = o;
    return 1;
}

static void arc_fmt_value(ArcBuf *b, const void *p, const ArcTD *td, int quoted, int depth) {
    if (depth > 24) { arc_buf_cstr(b, "..."); return; }
    switch (td->kind) {
    case ARC_K_INT: arc_buf_int(b, *(const int64_t *)p); return;
    case ARC_K_FLOAT: arc_buf_float(b, *(const double *)p); return;
    case ARC_K_BOOL: arc_buf_bool(b, *(const bool *)p); return;
    case ARC_K_STR: {
        const ArcStr *s = *(ArcStr *const *)p;
        if (quoted) arc_buf_quoted(b, s); else arc_buf_str(b, s);
        return;
    }
    case ARC_K_OPT: {
        if (td->is_heap) {
            if (!*(void *const *)p) { arc_buf_cstr(b, "nil"); return; }
            arc_fmt_value(b, p, td->elem, quoted, depth);
        } else {
            if (!*(const bool *)p) { arc_buf_cstr(b, "nil"); return; }
            arc_fmt_value(b, (const char *)p + td->opt_off, td->elem, quoted, depth);
        }
        return;
    }
    case ARC_K_ARR: {
        const ArcArr *a = *(ArcArr *const *)p;
        if (!arc_fmt_enter(b, a, depth)) return;
        arc_buf_putc(b, '[');
        for (int64_t i = 0; i < a->len; i++) {
            if (i) arc_buf_put(b, ", ", 2);
            arc_fmt_value(b, a->data + (size_t)i * a->esize, td->elem, 1, depth + 1);
        }
        arc_buf_putc(b, ']');
        return;
    }
    case ARC_K_MAP: {
        const ArcMap *m = *(ArcMap *const *)p;
        if (!arc_fmt_enter(b, m, depth)) return;
        arc_buf_putc(b, '[');
        if (m->len == 0) arc_buf_putc(b, ':');
        int first = 1;
        for (int64_t i = 0; i < m->nent; i++) {
            const char *e = m->ent + (size_t)i * m->esize;
            if (!e[8]) continue;
            if (!first) arc_buf_put(b, ", ", 2);
            first = 0;
            arc_fmt_value(b, e + m->koff, td->key, 1, depth + 1);
            arc_buf_put(b, ": ", 2);
            arc_fmt_value(b, e + m->voff, td->elem, 1, depth + 1);
        }
        arc_buf_putc(b, ']');
        return;
    }
    case ARC_K_STRUCT: {
        const char *o = *(const char *const *)p;
        if (!arc_fmt_enter(b, o, depth)) return;
        arc_buf_cstr(b, td->name);
        if (!td->nfields) { arc_buf_cstr(b, " {}"); return; }
        arc_buf_cstr(b, " { ");
        for (int i = 0; i < td->nfields; i++) {
            if (i) arc_buf_put(b, ", ", 2);
            arc_buf_cstr(b, td->fields[i].name);
            arc_buf_put(b, ": ", 2);
            arc_fmt_value(b, o + td->fields[i].offset, td->fields[i].td, 1, depth + 1);
        }
        arc_buf_cstr(b, " }");
        return;
    }
    case ARC_K_ENUM: {
        const char *o = *(const char *const *)p;
        if (!arc_fmt_enter(b, o, depth)) return;
        int64_t tag = ((const ArcEnumHdr *)o)->tag;
        arc_buf_cstr(b, td->vnames[tag]);
        int first = td->vfirst[tag], n = td->vcount[tag];
        if (!n) return;
        arc_buf_putc(b, '(');
        for (int i = 0; i < n; i++) {
            if (i) arc_buf_put(b, ", ", 2);
            arc_fmt_value(b, o + td->fields[first + i].offset, td->fields[first + i].td, 1, depth + 1);
        }
        arc_buf_putc(b, ')');
        return;
    }
    case ARC_K_ERR: {
        const ArcErr *e = *(ArcErr *const *)p;
        arc_buf_cstr(b, "error(");
        arc_buf_quoted(b, e->msg);
        arc_buf_putc(b, ')');
        return;
    }
    case ARC_K_RES: {
        const void *err;
        if (td->is_heap) {
            const void *v = *(void *const *)p;
            err = (v && ((const ArcObj *)v)->td == &arc_td_err) ? v : NULL;
            if (!err) {
                if (td->elem) arc_fmt_value(b, p, td->elem, quoted, depth);
                else arc_buf_cstr(b, "ok");
                return;
            }
        } else {
            err = *(void *const *)p;
            if (!err) { arc_fmt_value(b, (const char *)p + td->opt_off, td->elem, quoted, depth); return; }
        }
        arc_fmt_value(b, &err, &arc_td_err, quoted, depth);
        return;
    }
    case ARC_K_FN:
        arc_buf_cstr(b, "<fn>");
        return;
    }
}

/* ------------------------------------------------------------- strings -- */

static ArcStr *arc_str_alloc(int64_t len) {
    if (ARC_UNLIKELY(len < 0 || (uint64_t)len > ((uint64_t)1 << 48))) arc_capacity_overflow();
    ArcStr *s = (ArcStr *)arc_alloc(&arc_td_str, sizeof(ArcStr) + (size_t)len + 1);
    s->len = len;
    s->data = (const char *)(s + 1);
    ((char *)s->data)[len] = 0;
    return s;
}

static ArcStr *arc_str_new(const char *p, int64_t n) {
    ArcStr *s = arc_str_alloc(n);
    if (n) memcpy((char *)s->data, p, (size_t)n);
    return s;
}

static ArcStr *arc_str_from_buf(ArcBuf *b) {
    ArcStr *s = arc_str_new(b->p ? b->p : "", (int64_t)b->len);
    arc_buf_free(b);
    return s;
}

static ArcStr *arc_str_concat(const ArcStr *a, const ArcStr *b) {
    ArcStr *s = arc_str_alloc(a->len + b->len);
    memcpy((char *)s->data, a->data, (size_t)a->len);
    memcpy((char *)s->data + a->len, b->data, (size_t)b->len);
    return s;
}

static inline bool arc_str_eq(const ArcStr *a, const ArcStr *b) {
    return a == b || (a->len == b->len && memcmp(a->data, b->data, (size_t)a->len) == 0);
}

static int arc_str_cmp(const ArcStr *a, const ArcStr *b) {
    int64_t n = a->len < b->len ? a->len : b->len;
    int c = memcmp(a->data, b->data, (size_t)n);
    if (c) return c;
    return a->len < b->len ? -1 : (a->len > b->len ? 1 : 0);
}

static ArcStr *arc_str_of_value(const void *p, const ArcTD *td) {
    ArcBuf b;
    arc_buf_init(&b);
    arc_fmt_value(&b, p, td, 0, 0);
    return arc_str_from_buf(&b);
}
static ArcStr *arc_str_of_int(int64_t v) { ArcBuf b; arc_buf_init(&b); arc_buf_int(&b, v); return arc_str_from_buf(&b); }
static ArcStr *arc_str_of_float(double v) { ArcBuf b; arc_buf_init(&b); arc_buf_float(&b, v); return arc_str_from_buf(&b); }
static ArcStr *arc_str_of_bool(bool v) { return arc_str_new(v ? "true" : "false", v ? 4 : 5); }

static void arc_check_range(int64_t a, int64_t b, int64_t len, ArcLoc loc) {
    if (ARC_UNLIKELY(a < 0 || b > len || a > b))
        arc_panic_at(loc, "range %lld..%lld out of bounds for length %lld", (long long)a, (long long)b, (long long)len);
}

static ArcStr *arc_str_slice(const ArcStr *s, int64_t a, int64_t b, ArcLoc loc) {
    arc_check_range(a, b, s->len, loc);
    return arc_str_new(s->data + a, b - a);
}

static int64_t arc_str_find(const ArcStr *s, const ArcStr *sub) {
    if (sub->len == 0) return 0;
    if (sub->len > s->len) return -1;
    for (int64_t i = 0; i + sub->len <= s->len; i++)
        if (s->data[i] == sub->data[0] && memcmp(s->data + i, sub->data, (size_t)sub->len) == 0) return i;
    return -1;
}
static bool arc_str_contains(const ArcStr *s, const ArcStr *sub) { return arc_str_find(s, sub) >= 0; }
static bool arc_str_starts_with(const ArcStr *s, const ArcStr *p) {
    return p->len <= s->len && memcmp(s->data, p->data, (size_t)p->len) == 0;
}
static bool arc_str_ends_with(const ArcStr *s, const ArcStr *p) {
    return p->len <= s->len && memcmp(s->data + s->len - p->len, p->data, (size_t)p->len) == 0;
}

static ArcStr *arc_str_upper(const ArcStr *s) {
    ArcStr *r = arc_str_new(s->data, s->len);
    for (int64_t i = 0; i < r->len; i++) {
        char c = r->data[i];
        if (c >= 'a' && c <= 'z') ((char *)r->data)[i] = (char)(c - 32);
    }
    return r;
}
static ArcStr *arc_str_lower(const ArcStr *s) {
    ArcStr *r = arc_str_new(s->data, s->len);
    for (int64_t i = 0; i < r->len; i++) {
        char c = r->data[i];
        if (c >= 'A' && c <= 'Z') ((char *)r->data)[i] = (char)(c + 32);
    }
    return r;
}
static int arc_is_space(char c) { return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f'; }
static ArcStr *arc_str_trim(const ArcStr *s) {
    int64_t a = 0, b = s->len;
    while (a < b && arc_is_space(s->data[a])) a++;
    while (b > a && arc_is_space(s->data[b - 1])) b--;
    return arc_str_new(s->data + a, b - a);
}

static ArcStr *arc_str_replace(const ArcStr *s, const ArcStr *from, const ArcStr *to) {
    if (from->len == 0) return arc_str_new(s->data, s->len);
    ArcBuf b = {0};
    int64_t i = 0;
    while (i < s->len) {
        if (i + from->len <= s->len && memcmp(s->data + i, from->data, (size_t)from->len) == 0) {
            arc_buf_put(&b, to->data, (size_t)to->len);
            i += from->len;
        } else {
            arc_buf_putc(&b, s->data[i++]);
        }
    }
    return arc_str_from_buf(&b);
}

static ArcStr *arc_str_repeat(const ArcStr *s, int64_t n, ArcLoc loc) {
    if (n < 0) arc_panic_at(loc, "repeat count must be non-negative, got %lld", (long long)n);
    if (n > 0 && s->len > INT64_MAX / n)
        arc_panic_at(loc, "repeat result too large: %lld bytes x %lld", (long long)s->len, (long long)n);
    ArcStr *r = arc_str_alloc(s->len * n);
    for (int64_t i = 0; i < n; i++) memcpy((char *)r->data + i * s->len, s->data, (size_t)s->len);
    return r;
}

static int64_t arc_str_byte_at(const ArcStr *s, int64_t i, ArcLoc loc) {
    if (ARC_UNLIKELY((uint64_t)i >= (uint64_t)s->len))
        arc_panic_at(loc, "index out of bounds: the len is %lld but the index is %lld", (long long)s->len, (long long)i);
    return (unsigned char)s->data[i];
}

static int arc_utf8_len(unsigned char c) {
    if (c < 0x80) return 1;
    if ((c >> 5) == 0x6) return 2;
    if ((c >> 4) == 0xE) return 3;
    if ((c >> 3) == 0x1E) return 4;
    return 1;
}

static int64_t arc_str_char_count(const ArcStr *s) {
    int64_t n = 0;
    for (int64_t i = 0; i < s->len; i++)
        if (((unsigned char)s->data[i] & 0xC0) != 0x80) n++;
    return n;
}

static ArcStr *arc_chr(int64_t cp, ArcLoc loc) {
    char out[4];
    int n;
    if (cp < 0 || cp > 0x10FFFF) arc_panic_at(loc, "invalid character code %lld", (long long)cp);
    if (cp < 0x80) { out[0] = (char)cp; n = 1; }
    else if (cp < 0x800) { out[0] = (char)(0xC0 | (cp >> 6)); out[1] = (char)(0x80 | (cp & 0x3F)); n = 2; }
    else if (cp < 0x10000) {
        out[0] = (char)(0xE0 | (cp >> 12)); out[1] = (char)(0x80 | ((cp >> 6) & 0x3F));
        out[2] = (char)(0x80 | (cp & 0x3F)); n = 3;
    } else {
        out[0] = (char)(0xF0 | (cp >> 18)); out[1] = (char)(0x80 | ((cp >> 12) & 0x3F));
        out[2] = (char)(0x80 | ((cp >> 6) & 0x3F)); out[3] = (char)(0x80 | (cp & 0x3F)); n = 4;
    }
    return arc_str_new(out, n);
}

static int64_t arc_ord(const ArcStr *s, ArcLoc loc) {
    if (s->len == 0) arc_panic_at(loc, "ord() of empty string");
    const unsigned char *p = (const unsigned char *)s->data;
    int n = arc_utf8_len(p[0]);
    if (n > s->len) n = 1;
    if (n == 1) return p[0];
    int64_t cp = p[0] & (0xFF >> (n + 1));
    for (int i = 1; i < n; i++) cp = (cp << 6) | (p[i] & 0x3F);
    return cp;
}

static ArcOptInt arc_parse_int(const ArcStr *s) {
    ArcOptInt r = {false, 0};
    int64_t i = 0, n = s->len;
    while (i < n && arc_is_space(s->data[i])) i++;
    while (n > i && arc_is_space(s->data[n - 1])) n--;
    if (i >= n) return r;
    bool neg = false;
    if (s->data[i] == '+' || s->data[i] == '-') { neg = s->data[i] == '-'; i++; }
    if (i >= n) return r;
    uint64_t v = 0;
    for (; i < n; i++) {
        char c = s->data[i];
        if (c == '_') continue;
        if (c < '0' || c > '9') return r;
        uint64_t nv = v * 10 + (uint64_t)(c - '0');
        if (nv / 10 != v && v != 0) return r;
        v = nv;
        if (v > (uint64_t)INT64_MAX + (neg ? 1u : 0u)) return r;
    }
    r.has = true;
    r.val = neg ? (int64_t)(0 - v) : (int64_t)v;
    return r;
}

static ArcOptFloat arc_parse_float(const ArcStr *s) {
    ArcOptFloat r = {false, 0};
    char *end = NULL;
    const char *p = s->data;
    while (*p && arc_is_space(*p)) p++;
    if (!*p) return r;
    double v = strtod(p, &end);
    if (end == p) return r;
    while (*end && arc_is_space(*end)) end++;
    if (end != s->data + s->len) return r;
    r.has = true;
    r.val = v;
    return r;
}

/* -------------------------------------------------------------- arrays -- */

static ArcArr *arc_arr_new(const ArcTD *td, int64_t len, int64_t cap) {
    if (cap < len) cap = len;
    if (ARC_UNLIKELY(len < 0)) arc_capacity_overflow();
    size_t bytes = arc_mul_size((size_t)cap, td->elem->size);
    char *data = cap ? (char *)arc_xcalloc(bytes) : NULL;
    ArcArr *a = (ArcArr *)arc_alloc(td, sizeof(ArcArr));
    a->esize = td->elem->size;
    a->len = len;
    a->cap = cap;
    a->data = data;
    arc_alloc_bytes += bytes;
    return a;
}

static void arc_arr_reserve(ArcArr *a, int64_t need) {
    if (need <= a->cap) return;
    int64_t nc = a->cap ? a->cap : 4;
    while (nc < need) nc = nc > INT64_MAX / 2 ? need : nc * 2;
    a->data = (char *)arc_xrealloc(a->data, arc_mul_size((size_t)nc, a->esize));
    memset(a->data + (size_t)a->cap * a->esize, 0, (size_t)(nc - a->cap) * a->esize);
    arc_alloc_bytes += (size_t)(nc - a->cap) * a->esize;
    a->cap = nc;
}

static void arc_arr_grow(ArcArr *a) { arc_arr_reserve(a, a->len + 1); }

#define ARC_PUSH(T, a, v) do { ArcArr *_a = (a); if (ARC_UNLIKELY(_a->len == _a->cap)) arc_arr_grow(_a); ((T *)_a->data)[_a->len++] = (v); } while (0)

ARC_COLD ARC_NORET static void arc_index_fail(int64_t i, int64_t len, ArcLoc loc) {
    arc_panic_at(loc, "index out of bounds: the len is %lld but the index is %lld", (long long)len, (long long)i);
}

static inline int64_t arc_idx(const ArcArr *a, int64_t i, ArcLoc loc) {
    if (ARC_UNLIKELY((uint64_t)i >= (uint64_t)a->len)) arc_index_fail(i, a->len, loc);
    return i;
}

static inline int64_t arc_pop_idx(ArcArr *a, ArcLoc loc) {
    if (ARC_UNLIKELY(a->len == 0)) arc_panic_at(loc, "pop() called on an empty array");
    return --a->len;
}

static inline int64_t arc_last_idx(const ArcArr *a, ArcLoc loc) {
    if (ARC_UNLIKELY(a->len == 0)) arc_panic_at(loc, "last() called on an empty array");
    return a->len - 1;
}

/* opens a gap at i, returns i */
static int64_t arc_arr_insert_at(ArcArr *a, int64_t i, ArcLoc loc) {
    if (ARC_UNLIKELY(i < 0 || i > a->len))
        arc_panic_at(loc, "insertion index (is %lld) should be <= len (is %lld)", (long long)i, (long long)a->len);
    arc_arr_grow(a);
    memmove(a->data + (size_t)(i + 1) * a->esize, a->data + (size_t)i * a->esize, (size_t)(a->len - i) * a->esize);
    a->len++;
    return i;
}

static void arc_arr_remove_at(ArcArr *a, int64_t i, void *out, ArcLoc loc) {
    if (ARC_UNLIKELY((uint64_t)i >= (uint64_t)a->len))
        arc_panic_at(loc, "removal index (is %lld) should be < len (is %lld)", (long long)i, (long long)a->len);
    memcpy(out, a->data + (size_t)i * a->esize, a->esize);
    memmove(a->data + (size_t)i * a->esize, a->data + (size_t)(i + 1) * a->esize, (size_t)(a->len - i - 1) * a->esize);
    a->len--;
    memset(a->data + (size_t)a->len * a->esize, 0, a->esize);
}

static ArcArr *arc_arr_slice(const ArcArr *a, int64_t s, int64_t e, ArcLoc loc) {
    arc_check_range(s, e, a->len, loc);
    ArcArr *r = arc_arr_new(a->h.td, e - s, e - s);
    if (e > s) memcpy(r->data, a->data + (size_t)s * a->esize, (size_t)(e - s) * a->esize);
    return r;
}

static void arc_arr_clear(ArcArr *a) {
    if (a->data) memset(a->data, 0, (size_t)a->len * a->esize);
    a->len = 0;
}

static void arc_arr_reverse(ArcArr *a) {
    char tmp[64];
    size_t es = a->esize;
    for (int64_t i = 0, j = a->len - 1; i < j; i++, j--) {
        memcpy(tmp, a->data + (size_t)i * es, es);
        memcpy(a->data + (size_t)i * es, a->data + (size_t)j * es, es);
        memcpy(a->data + (size_t)j * es, tmp, es);
    }
}

static int arc_cmp_int(const void *x, const void *y) {
    int64_t a = *(const int64_t *)x, b = *(const int64_t *)y;
    return (a > b) - (a < b);
}
static int arc_cmp_float(const void *x, const void *y) {
    double a = *(const double *)x, b = *(const double *)y;
    return (a > b) - (a < b);
}
static int arc_cmp_strp(const void *x, const void *y) {
    return arc_str_cmp(*(ArcStr *const *)x, *(ArcStr *const *)y);
}
static int arc_cmp_bool(const void *x, const void *y) {
    return (int)*(const bool *)x - (int)*(const bool *)y;
}

/* LSD radix sort on 64-bit keys; passes where every key shares the byte are skipped. */
static void arc_radix_u64(uint64_t *a, int64_t n) {
    if (n < 48) {
        for (int64_t i = 1; i < n; i++) {
            uint64_t v = a[i];
            int64_t j = i - 1;
            while (j >= 0 && a[j] > v) { a[j + 1] = a[j]; j--; }
            a[j + 1] = v;
        }
        return;
    }
    static size_t cnt[8][256];
    memset(cnt, 0, sizeof cnt);
    for (int64_t i = 0; i < n; i++) {
        uint64_t k = a[i];
        for (int b = 0; b < 8; b++) cnt[b][(k >> (8 * b)) & 255]++;
    }
    uint64_t *tmp = (uint64_t *)arc_xmalloc((size_t)n * sizeof(uint64_t));
    uint64_t *from = a, *to = tmp;
    for (int b = 0; b < 8; b++) {
        size_t *c = cnt[b];
        if (c[(a[0] >> (8 * b)) & 255] == (size_t)n) continue;
        size_t pos[256], sum = 0;
        for (int i = 0; i < 256; i++) { pos[i] = sum; sum += c[i]; }
        for (int64_t i = 0; i < n; i++) {
            uint64_t k = from[i];
            to[pos[(k >> (8 * b)) & 255]++] = k;
        }
        uint64_t *t = from; from = to; to = t;
    }
    if (from != a) memcpy(a, from, (size_t)n * sizeof(uint64_t));
    free(tmp);
}

static void arc_sort_i64(int64_t *a, int64_t n) {
    uint64_t *u = (uint64_t *)a;
    for (int64_t i = 0; i < n; i++) u[i] ^= 0x8000000000000000ULL;
    arc_radix_u64(u, n);
    for (int64_t i = 0; i < n; i++) u[i] ^= 0x8000000000000000ULL;
}

static void arc_sort_f64(double *a, int64_t n) {
    uint64_t *u = (uint64_t *)arc_xmalloc((size_t)n * sizeof(uint64_t));
    for (int64_t i = 0; i < n; i++) {
        uint64_t k;
        memcpy(&k, &a[i], 8);
        u[i] = (k >> 63) ? ~k : (k | 0x8000000000000000ULL);
    }
    arc_radix_u64(u, n);
    for (int64_t i = 0; i < n; i++) {
        uint64_t k = u[i];
        k = (k >> 63) ? (k & 0x7FFFFFFFFFFFFFFFULL) : ~k;
        memcpy(&a[i], &k, 8);
    }
    free(u);
}

static void arc_arr_sort(ArcArr *a) {
    if (a->len < 2) return;
    switch (a->h.td->elem->kind) {
    case ARC_K_INT: arc_sort_i64((int64_t *)a->data, a->len); break;
    case ARC_K_FLOAT: arc_sort_f64((double *)a->data, a->len); break;
    case ARC_K_STR: qsort(a->data, (size_t)a->len, a->esize, arc_cmp_strp); break;
    case ARC_K_BOOL: qsort(a->data, (size_t)a->len, a->esize, arc_cmp_bool); break;
    default: break;
    }
}

static bool arc_value_eq(const void *x, const void *y, const ArcTD *td) {
    switch (td->kind) {
    case ARC_K_INT: return *(const int64_t *)x == *(const int64_t *)y;
    case ARC_K_FLOAT: return *(const double *)x == *(const double *)y;
    case ARC_K_BOOL: return *(const bool *)x == *(const bool *)y;
    case ARC_K_STR: return arc_str_eq(*(ArcStr *const *)x, *(ArcStr *const *)y);
    default: return *(void *const *)x == *(void *const *)y;
    }
}

static int64_t arc_arr_index_of(const ArcArr *a, const void *v) {
    const ArcTD *et = a->h.td->elem;
    for (int64_t i = 0; i < a->len; i++)
        if (arc_value_eq(a->data + (size_t)i * a->esize, v, et)) return i;
    return -1;
}

static ArcStr *arc_arr_join(const ArcArr *a, const ArcStr *sep) {
    ArcBuf b = {0};
    ArcStr **d = (ArcStr **)a->data;
    for (int64_t i = 0; i < a->len; i++) {
        if (i) arc_buf_str(&b, sep);
        arc_buf_str(&b, d[i]);
    }
    return arc_str_from_buf(&b);
}

static ArcArr *arc_str_split(const ArcStr *s, const ArcStr *sep) {
    arc_gc_inhibit++;
    ArcArr *r = arc_arr_new(&arc_td_arr_str, 0, 4);
    if (sep->len == 0) {
        for (int64_t i = 0; i < s->len;) {
            int n = arc_utf8_len((unsigned char)s->data[i]);
            if (i + n > s->len) n = (int)(s->len - i);
            ArcStr *part = arc_str_new(s->data + i, n);
            ARC_PUSH(ArcStr *, r, part);
            i += n;
        }
    } else {
        int64_t start = 0;
        for (int64_t i = 0; i + sep->len <= s->len;) {
            if (memcmp(s->data + i, sep->data, (size_t)sep->len) == 0) {
                ArcStr *part = arc_str_new(s->data + start, i - start);
                ARC_PUSH(ArcStr *, r, part);
                i += sep->len;
                start = i;
            } else {
                i++;
            }
        }
        ArcStr *last = arc_str_new(s->data + start, s->len - start);
        ARC_PUSH(ArcStr *, r, last);
    }
    arc_gc_inhibit--;
    return r;
}

static ArcArr *arc_str_chars(const ArcStr *s) {
    ArcStr empty = {{0, &arc_td_str, ARC_F_STATIC, 0}, 0, ""};
    return arc_str_split(s, &empty);
}

static ArcArr *arc_str_lines(const ArcStr *s) {
    arc_gc_inhibit++;
    ArcArr *r = arc_arr_new(&arc_td_arr_str, 0, 4);
    int64_t start = 0;
    for (int64_t i = 0; i < s->len; i++) {
        if (s->data[i] == '\n') {
            int64_t end = i;
            if (end > start && s->data[end - 1] == '\r') end--;
            ArcStr *part = arc_str_new(s->data + start, end - start);
            ARC_PUSH(ArcStr *, r, part);
            start = i + 1;
        }
    }
    if (start < s->len) {
        ArcStr *part = arc_str_new(s->data + start, s->len - start);
        ARC_PUSH(ArcStr *, r, part);
    }
    arc_gc_inhibit--;
    return r;
}

static ArcArr *arc_str_split_ws(const ArcStr *s) {
    arc_gc_inhibit++;
    ArcArr *r = arc_arr_new(&arc_td_arr_str, 0, 4);
    int64_t i = 0;
    while (i < s->len) {
        while (i < s->len && arc_is_space(s->data[i])) i++;
        int64_t st = i;
        while (i < s->len && !arc_is_space(s->data[i])) i++;
        if (i > st) {
            ArcStr *part = arc_str_new(s->data + st, i - st);
            ARC_PUSH(ArcStr *, r, part);
        }
    }
    arc_gc_inhibit--;
    return r;
}

/* ---------------------------------------------------------------- maps -- */

static uint64_t arc_hash_u64(uint64_t x) {
    x ^= x >> 30; x *= 0xbf58476d1ce4e5b9ULL;
    x ^= x >> 27; x *= 0x94d049bb133111ebULL;
    x ^= x >> 31;
    return x;
}

/* Per-process random keys make hash values unpredictable, so attackers cannot
 * precompute colliding keys (hash flooding). Set by arc_init_random(). */
static uint64_t arc_hash_k0 = 0x0706050403020100ULL, arc_hash_k1 = 0x0f0e0d0c0b0a0908ULL;

#define ARC_ROTL(x, b) (uint64_t)(((x) << (b)) | ((x) >> (64 - (b))))
#define ARC_SIPROUND                                                         \
    do {                                                                     \
        v0 += v1; v1 = ARC_ROTL(v1, 13); v1 ^= v0; v0 = ARC_ROTL(v0, 32);    \
        v2 += v3; v3 = ARC_ROTL(v3, 16); v3 ^= v2;                           \
        v0 += v3; v3 = ARC_ROTL(v3, 21); v3 ^= v0;                           \
        v2 += v1; v1 = ARC_ROTL(v1, 17); v1 ^= v2; v2 = ARC_ROTL(v2, 32);    \
    } while (0)

/* SipHash-1-3 (the keyed hash Rust's HashMap uses) */
static uint64_t arc_hash_bytes(const char *p, int64_t n) {
    uint64_t v0 = arc_hash_k0 ^ 0x736f6d6570736575ULL, v1 = arc_hash_k1 ^ 0x646f72616e646f6dULL;
    uint64_t v2 = arc_hash_k0 ^ 0x6c7967656e657261ULL, v3 = arc_hash_k1 ^ 0x7465646279746573ULL;
    const unsigned char *in = (const unsigned char *)p;
    size_t len = (size_t)n, i = 0;
    for (; i + 8 <= len; i += 8) {
        uint64_t m;
        memcpy(&m, in + i, 8);
        v3 ^= m;
        ARC_SIPROUND;
        v0 ^= m;
    }
    uint64_t last = (uint64_t)len << 56;
    for (size_t j = 0; i + j < len; j++) last |= (uint64_t)in[i + j] << (8 * j);
    v3 ^= last;
    ARC_SIPROUND;
    v0 ^= last;
    v2 ^= 0xff;
    ARC_SIPROUND;
    ARC_SIPROUND;
    ARC_SIPROUND;
    return v0 ^ v1 ^ v2 ^ v3;
}

static inline uint64_t arc_hash_scalar(uint64_t x) {
    return arc_hash_u64(arc_hash_u64(x ^ arc_hash_k0) ^ arc_hash_k1);
}

static uint64_t arc_hash_key(const void *k, const ArcTD *td) {
    switch (td->kind) {
    case ARC_K_STR: { const ArcStr *s = *(ArcStr *const *)k; return arc_hash_bytes(s->data, s->len); }
    case ARC_K_BOOL: return arc_hash_scalar(*(const bool *)k);
    case ARC_K_FLOAT: { double d = *(const double *)k; if (d == 0) d = 0; uint64_t u; memcpy(&u, &d, 8); return arc_hash_scalar(u); }
    default: return arc_hash_scalar(*(const uint64_t *)k);
    }
}

static size_t arc_round8(size_t n) { return (n + 7) & ~(size_t)7; }

static ArcMap *arc_map_new(const ArcTD *td) {
    ArcMap *m = (ArcMap *)arc_alloc(td, sizeof(ArcMap));
    m->ksize = td->key->size;
    m->vsize = td->elem->size;
    m->koff = 16; /* [hash:8][live:1 + pad] */
    m->voff = m->koff + arc_round8(m->ksize);
    m->esize = m->voff + arc_round8(m->vsize);
    return m;
}

static inline char *arc_map_ent(const ArcMap *m, int64_t i) { return m->ent + (size_t)i * m->esize; }

static int64_t arc_map_find(const ArcMap *m, const void *key, uint64_t h, int64_t *slot_out) {
    if (!m->icap) { if (slot_out) *slot_out = -1; return -1; }
    const ArcTD *kt = m->h.td->key;
    uint64_t mask = (uint64_t)m->icap - 1;
    uint64_t i = h & mask;
    int64_t first_free = -1;
    for (;;) {
        int64_t e = m->idx[i];
        if (e == -1) {
            if (slot_out) *slot_out = first_free >= 0 ? first_free : (int64_t)i;
            return -1;
        }
        if (e == -2) {
            if (first_free < 0) first_free = (int64_t)i;
        } else {
            char *ent = arc_map_ent(m, e);
            if (*(uint64_t *)ent == h && arc_value_eq(ent + m->koff, key, kt)) {
                if (slot_out) *slot_out = (int64_t)i;
                return e;
            }
        }
        i = (i + 1) & mask;
    }
}

static void arc_map_rebuild(ArcMap *m, int64_t need) {
    /* compact entries */
    int64_t w = 0;
    for (int64_t r = 0; r < m->nent; r++) {
        char *e = arc_map_ent(m, r);
        if (!e[8]) continue;
        if (w != r) memcpy(arc_map_ent(m, w), e, m->esize);
        w++;
    }
    if (w < m->nent) memset(arc_map_ent(m, w), 0, (size_t)(m->nent - w) * m->esize);
    m->nent = w;
    int64_t icap = 8;
    while (icap < need * 2) icap *= 2;
    if (icap != m->icap) {
        arc_alloc_bytes += (size_t)icap * sizeof(int64_t);
        free(m->idx);
        m->idx = (int64_t *)arc_xmalloc((size_t)icap * sizeof(int64_t));
        m->icap = icap;
    }
    memset(m->idx, 0xFF, (size_t)icap * sizeof(int64_t)); /* all -1 */
    uint64_t mask = (uint64_t)icap - 1;
    for (int64_t k = 0; k < m->nent; k++) {
        uint64_t i = *(uint64_t *)arc_map_ent(m, k) & mask;
        while (m->idx[i] != -1) i = (i + 1) & mask;
        m->idx[i] = k;
    }
}

static void *arc_map_get(const ArcMap *m, const void *key) {
    int64_t e = arc_map_find(m, key, arc_hash_key(key, m->h.td->key), NULL);
    return e < 0 ? NULL : arc_map_ent(m, e) + m->voff;
}

ARC_COLD ARC_NORET static void arc_map_key_fail(const ArcMap *m, const void *key, ArcLoc loc) {
    ArcBuf b = {0};
    arc_fmt_value(&b, key, m->h.td->key, 1, 0);
    arc_buf_putc(&b, 0);
    arc_panic_at(loc, "key not found in map: %s", b.p);
}

static void *arc_map_must(const ArcMap *m, const void *key, ArcLoc loc) {
    void *p = arc_map_get(m, key);
    if (ARC_UNLIKELY(!p)) arc_map_key_fail(m, key, loc);
    return p;
}

/* returns pointer to the value slot, inserting a zeroed entry if needed */
static void *arc_map_slot(ArcMap *m, const void *key) {
    uint64_t h = arc_hash_key(key, m->h.td->key);
    int64_t slot;
    int64_t e = arc_map_find(m, key, h, &slot);
    if (e >= 0) return arc_map_ent(m, e) + m->voff;
    if ((m->nent + 1) * 4 > m->icap * 3 || slot < 0) {
        arc_map_rebuild(m, m->len + 1);
        arc_map_find(m, key, h, &slot);
    }
    if (m->nent == m->capent) {
        int64_t nc = m->capent ? m->capent * 2 : 8;
        m->ent = (char *)arc_xrealloc(m->ent, arc_mul_size((size_t)nc, m->esize));
        memset(m->ent + (size_t)m->capent * m->esize, 0, (size_t)(nc - m->capent) * m->esize);
        arc_alloc_bytes += (size_t)(nc - m->capent) * m->esize;
        m->capent = nc;
    }
    int64_t k = m->nent++;
    char *ent = arc_map_ent(m, k);
    *(uint64_t *)ent = h;
    ent[8] = 1;
    memcpy(ent + m->koff, key, m->ksize);
    memset(ent + m->voff, 0, m->vsize);
    m->idx[slot] = k;
    m->len++;
    return ent + m->voff;
}

static bool arc_map_remove(ArcMap *m, const void *key) {
    int64_t slot;
    int64_t e = arc_map_find(m, key, arc_hash_key(key, m->h.td->key), &slot);
    if (e < 0) return false;
    char *ent = arc_map_ent(m, e);
    ent[8] = 0;
    memset(ent + m->koff, 0, m->esize - m->koff);
    m->idx[slot] = -2;
    m->len--;
    return true;
}

static void arc_map_clear(ArcMap *m) {
    if (m->ent) memset(m->ent, 0, (size_t)m->nent * m->esize);
    m->nent = 0;
    m->len = 0;
    if (m->idx) memset(m->idx, 0xFF, (size_t)m->icap * sizeof(int64_t));
}

/* next live entry index >= i, or -1 */
static inline int64_t arc_map_next(const ArcMap *m, int64_t i) {
    for (; i < m->nent; i++)
        if (arc_map_ent(m, i)[8]) return i;
    return -1;
}
static inline void *arc_map_ekey(const ArcMap *m, int64_t i) { return arc_map_ent(m, i) + m->koff; }
static inline void *arc_map_eval(const ArcMap *m, int64_t i) { return arc_map_ent(m, i) + m->voff; }

static ArcArr *arc_map_keys(const ArcMap *m, const ArcTD *arrtd) {
    ArcArr *r = arc_arr_new(arrtd, m->len, m->len);
    int64_t w = 0;
    for (int64_t i = 0; i < m->nent; i++) {
        char *e = arc_map_ent(m, i);
        if (!e[8]) continue;
        memcpy(r->data + (size_t)w++ * r->esize, e + m->koff, m->ksize);
    }
    return r;
}

static ArcArr *arc_map_values(const ArcMap *m, const ArcTD *arrtd) {
    ArcArr *r = arc_arr_new(arrtd, m->len, m->len);
    int64_t w = 0;
    for (int64_t i = 0; i < m->nent; i++) {
        char *e = arc_map_ent(m, i);
        if (!e[8]) continue;
        memcpy(r->data + (size_t)w++ * r->esize, e + m->voff, m->vsize);
    }
    return r;
}

/* Structural equality. Iterative (no C-stack limit) and cycle-safe: a pair of
 * objects already under comparison is assumed equal (bisimulation). */
typedef struct { const void *x, *y; const ArcTD *td; } ArcEqItem;
typedef struct { const void *a, *b; } ArcEqPair;

static bool arc_eq_seen(ArcEqPair **set, size_t *cap, size_t *count, const void *a, const void *b) {
    if (*count * 2 >= *cap) {
        size_t ncap = *cap ? *cap * 2 : 64;
        ArcEqPair *ns = (ArcEqPair *)arc_xcalloc(ncap * sizeof(ArcEqPair));
        for (size_t i = 0; i < *cap; i++) {
            if (!(*set)[i].a) continue;
            size_t h = (size_t)arc_hash_u64((uint64_t)(uintptr_t)(*set)[i].a * 31 + (uint64_t)(uintptr_t)(*set)[i].b) & (ncap - 1);
            while (ns[h].a) h = (h + 1) & (ncap - 1);
            ns[h] = (*set)[i];
        }
        free(*set);
        *set = ns;
        *cap = ncap;
    }
    size_t h = (size_t)arc_hash_u64((uint64_t)(uintptr_t)a * 31 + (uint64_t)(uintptr_t)b) & (*cap - 1);
    while ((*set)[h].a) {
        if ((*set)[h].a == a && (*set)[h].b == b) return true;
        h = (h + 1) & (*cap - 1);
    }
    (*set)[h].a = a;
    (*set)[h].b = b;
    (*count)++;
    return false;
}

static bool arc_deep_eq(const void *x, const void *y, const ArcTD *td, int unused) {
    (void)unused;
    ArcEqItem *st = NULL;
    size_t n = 0, cap = 0;
    ArcEqPair *seen = NULL;
    size_t scap = 0, scount = 0;
    bool eq = true;
#define ARC_EQ_PUSH(X, Y, T) do { \
        if (n == cap) { cap = cap ? cap * 2 : 64; st = (ArcEqItem *)arc_xrealloc(st, cap * sizeof(ArcEqItem)); } \
        st[n].x = (X); st[n].y = (Y); st[n].td = (T); n++; } while (0)
    ARC_EQ_PUSH(x, y, td);
    while (n && eq) {
        ArcEqItem it = st[--n];
        const ArcTD *t = it.td;
        switch (t->kind) {
        case ARC_K_INT: case ARC_K_FLOAT: case ARC_K_BOOL: case ARC_K_STR:
            eq = arc_value_eq(it.x, it.y, t);
            break;
        case ARC_K_OPT:
            if (t->is_heap) {
                const void *a = *(void *const *)it.x, *b = *(void *const *)it.y;
                if (!a || !b) eq = a == b;
                else ARC_EQ_PUSH(it.x, it.y, t->elem);
            } else {
                bool ha = *(const bool *)it.x, hb = *(const bool *)it.y;
                if (!ha || !hb) eq = ha == hb;
                else ARC_EQ_PUSH((const char *)it.x + t->opt_off, (const char *)it.y + t->opt_off, t->elem);
            }
            break;
        case ARC_K_ARR: {
            const ArcArr *a = *(ArcArr *const *)it.x, *b = *(ArcArr *const *)it.y;
            if (a == b || arc_eq_seen(&seen, &scap, &scount, a, b)) break;
            if (a->len != b->len) { eq = false; break; }
            for (int64_t i = a->len - 1; i >= 0; i--)
                ARC_EQ_PUSH(a->data + (size_t)i * a->esize, b->data + (size_t)i * b->esize, t->elem);
            break;
        }
        case ARC_K_MAP: {
            const ArcMap *a = *(ArcMap *const *)it.x, *b = *(ArcMap *const *)it.y;
            if (a == b || arc_eq_seen(&seen, &scap, &scount, a, b)) break;
            if (a->len != b->len) { eq = false; break; }
            for (int64_t i = 0; i < a->nent && eq; i++) {
                const char *e = a->ent + (size_t)i * a->esize;
                if (!e[8]) continue;
                void *bv = arc_map_get(b, e + a->koff);
                if (!bv) eq = false;
                else ARC_EQ_PUSH(e + a->voff, bv, t->elem);
            }
            break;
        }
        case ARC_K_ENUM: {
            const char *a = *(const char *const *)it.x, *b = *(const char *const *)it.y;
            if (a == b || arc_eq_seen(&seen, &scap, &scount, a, b)) break;
            int64_t tag = ((const ArcEnumHdr *)a)->tag;
            if (tag != ((const ArcEnumHdr *)b)->tag) { eq = false; break; }
            for (int i = t->vfirst[tag] + t->vcount[tag] - 1; i >= t->vfirst[tag]; i--)
                ARC_EQ_PUSH(a + t->fields[i].offset, b + t->fields[i].offset, t->fields[i].td);
            break;
        }
        case ARC_K_ERR: {
            const ArcErr *a = *(ArcErr *const *)it.x, *b = *(ArcErr *const *)it.y;
            eq = a == b || arc_str_eq(a->msg, b->msg);
            break;
        }
        case ARC_K_FN:
            eq = *(void *const *)it.x == *(void *const *)it.y;
            break;
        case ARC_K_RES: {
            const void *ea, *eb;
            if (t->is_heap) {
                const void *a = *(void *const *)it.x, *b = *(void *const *)it.y;
                ea = (a && ((const ArcObj *)a)->td == &arc_td_err) ? a : NULL;
                eb = (b && ((const ArcObj *)b)->td == &arc_td_err) ? b : NULL;
                if (!ea && !eb) { if (t->elem) ARC_EQ_PUSH(it.x, it.y, t->elem); break; }
            } else {
                ea = *(void *const *)it.x;
                eb = *(void *const *)it.y;
                if (!ea && !eb) { ARC_EQ_PUSH((const char *)it.x + t->opt_off, (const char *)it.y + t->opt_off, t->elem); break; }
            }
            if (!ea || !eb) { eq = false; break; }
            eq = arc_str_eq(((const ArcErr *)ea)->msg, ((const ArcErr *)eb)->msg);
            break;
        }
        case ARC_K_STRUCT: {
            const char *a = *(const char *const *)it.x, *b = *(const char *const *)it.y;
            if (a == b || arc_eq_seen(&seen, &scap, &scount, a, b)) break;
            for (int i = t->nfields - 1; i >= 0; i--)
                ARC_EQ_PUSH(a + t->fields[i].offset, b + t->fields[i].offset, t->fields[i].td);
            break;
        }
        default:
            eq = false;
        }
    }
#undef ARC_EQ_PUSH
    free(st);
    free(seen);
    return eq;
}

/* ------------------------------------------------------------ integers -- */

ARC_COLD ARC_NORET static void arc_div_zero(ArcLoc loc) { arc_panic_at(loc, "attempt to divide by zero"); }

static inline int64_t arc_div(int64_t a, int64_t b, ArcLoc loc) {
    if (ARC_UNLIKELY(b == 0)) arc_div_zero(loc);
    if (ARC_UNLIKELY(b == -1)) return (int64_t)(0 - (uint64_t)a);
    return a / b;
}
static inline int64_t arc_mod(int64_t a, int64_t b, ArcLoc loc) {
    if (ARC_UNLIKELY(b == 0)) arc_panic_at(loc, "attempt to calculate the remainder with a divisor of zero");
    if (ARC_UNLIKELY(b == -1)) return 0;
    return a % b;
}
static inline int64_t arc_shl(int64_t a, int64_t b, ArcLoc loc) {
    if (ARC_UNLIKELY(b < 0)) arc_panic_at(loc, "negative shift amount %lld", (long long)b);
    return b >= 64 ? 0 : (int64_t)((uint64_t)a << b);
}
static inline int64_t arc_shr(int64_t a, int64_t b, ArcLoc loc) {
    if (ARC_UNLIKELY(b < 0)) arc_panic_at(loc, "negative shift amount %lld", (long long)b);
    return b >= 64 ? (a < 0 ? -1 : 0) : (a >> b);
}
static inline int64_t arc_iabs(int64_t a) { return a < 0 ? (int64_t)(0 - (uint64_t)a) : a; }
static inline int64_t arc_imin(int64_t a, int64_t b) { return a < b ? a : b; }
static inline int64_t arc_imax(int64_t a, int64_t b) { return a > b ? a : b; }
static inline double arc_fmin(double a, double b) { return a < b ? a : b; }
static inline double arc_fmax(double a, double b) { return a > b ? a : b; }
static inline int64_t arc_ipow(int64_t b, int64_t e, ArcLoc loc) {
    if (e < 0) arc_panic_at(loc, "ipow() with negative exponent %lld", (long long)e);
    uint64_t r = 1, x = (uint64_t)b;
    while (e) { if (e & 1) r *= x; x *= x; e >>= 1; }
    return (int64_t)r;
}
static inline int64_t arc_f2i(double v) {
    /* saturating, like Rust's `as`: NaN -> 0 */
    if (v != v) return 0;
    if (v >= 9223372036854775807.0) return INT64_MAX;
    if (v <= -9223372036854775808.0) return INT64_MIN;
    return (int64_t)v;
}
static inline int64_t arc_check_len(int64_t n, ArcLoc loc) {
    if (ARC_UNLIKELY(n < 0)) arc_panic_at(loc, "array length must be non-negative, got %lld", (long long)n);
    if (ARC_UNLIKELY(n > ((int64_t)1 << 40))) arc_panic_at(loc, "array length %lld is too large", (long long)n);
    return n;
}

static ArcErr *arc_err_new(ArcStr *msg) {
    ArcErr *e = (ArcErr *)arc_alloc(&arc_td_err, sizeof(ArcErr));
    e->msg = msg;
    return e;
}
/* error from a C string + errno-style detail; GC-safe (inhibits collection) */
static ArcErr *arc_err_fmt(const char *what, const ArcStr *path, int errnum) {
    ArcBuf b;
    arc_buf_init(&b);
    arc_buf_cstr(&b, what);
    if (path) { arc_buf_cstr(&b, " '"); arc_buf_str(&b, path); arc_buf_putc(&b, '\''); }
    if (errnum) { arc_buf_cstr(&b, ": "); arc_buf_cstr(&b, strerror(errnum)); }
    arc_gc_inhibit++;
    ArcErr *e = arc_err_new(arc_str_from_buf(&b));
    arc_gc_inhibit--;
    return e;
}
static inline bool arc_is_err(const void *p) { return p && ((const ArcObj *)p)->td == &arc_td_err; }

ARC_COLD ARC_NORET static void arc_unwrap_err_fail(const ArcErr *e, ArcLoc loc) {
    arc_panic_at(loc, "called unwrap() on an error: %s", e->msg->data);
}

/* `fn main() -> !void`: report a returned error */
static int64_t arc_main_result(ArcErr *e) {
    if (!e) return 0;
    fflush(stdout);
    fprintf(stderr, "error: %s\n", e->msg->data);
    return 1;
}

/* stable merge sort with a user comparator; collection is paused because
 * elements move through a scratch buffer the GC cannot see */
static void arc_arr_sort_by(ArcArr *a, ArcClo *f, bool (*less)(ArcClo *, const void *, const void *)) {
    int64_t n = a->len;
    if (n < 2) return;
    size_t es = a->esize;
    char *tmp = (char *)arc_xmalloc(arc_mul_size((size_t)n, es));
    arc_gc_inhibit++;
    for (int64_t width = 1; width < n; width *= 2) {
        for (int64_t lo = 0; lo < n; lo += 2 * width) {
            int64_t mid = lo + width < n ? lo + width : n;
            int64_t hi = lo + 2 * width < n ? lo + 2 * width : n;
            int64_t i = lo, j = mid, k = lo;
            while (i < mid && j < hi) {
                if (less(f, a->data + (size_t)j * es, a->data + (size_t)i * es)) memcpy(tmp + (size_t)k++ * es, a->data + (size_t)j++ * es, es);
                else memcpy(tmp + (size_t)k++ * es, a->data + (size_t)i++ * es, es);
            }
            while (i < mid) memcpy(tmp + (size_t)k++ * es, a->data + (size_t)i++ * es, es);
            while (j < hi) memcpy(tmp + (size_t)k++ * es, a->data + (size_t)j++ * es, es);
        }
        memcpy(a->data, tmp, (size_t)n * es);
    }
    arc_gc_inhibit--;
    free(tmp);
}

ARC_COLD ARC_NORET static void arc_unwrap_fail(ArcLoc loc) {
    arc_panic_at(loc, "called unwrap() on a nil value");
}

/* ----------------------------------------------------------------- I/O -- */

static ArcBuf arc_pb; /* print buffer */

static void arc_out(FILE *f) {
    fwrite(arc_pb.p, 1, arc_pb.len, f);
    arc_pb.len = 0;
    if (f == stderr) fflush(stderr);
}

static ArcStr *arc_read_line(void) {
    fflush(stdout);
    ArcBuf b = {0};
    int c, any = 0;
    while ((c = getchar()) != EOF) {
        any = 1;
        if (c == '\n') break;
        arc_buf_putc(&b, (char)c);
    }
    if (!any) { arc_buf_free(&b); return NULL; }
    if (b.len && b.p[b.len - 1] == '\r') b.len--;
    return arc_str_from_buf(&b);
}

/* C APIs stop at the first NUL byte, Arc strings do not: reject such strings */
static inline bool arc_has_nul(const ArcStr *s) { return memchr(s->data, 0, (size_t)s->len) != NULL; }

static ArcStr *arc_read_file(const ArcStr *path) {
    if (arc_has_nul(path)) return NULL;
    FILE *f = fopen(path->data, "rb");
    if (!f) return NULL;
    ArcBuf b = {0};
    char tmp[65536];
    size_t n;
    while ((n = fread(tmp, 1, sizeof tmp, f)) > 0) arc_buf_put(&b, tmp, n);
    fclose(f);
    return arc_str_from_buf(&b);
}

static bool arc_write_file(const ArcStr *path, const ArcStr *data, int append) {
    if (arc_has_nul(path)) return false;
    FILE *f = fopen(path->data, append ? "ab" : "wb");
    if (!f) return false;
    size_t n = fwrite(data->data, 1, (size_t)data->len, f);
    return fclose(f) == 0 && n == (size_t)data->len;
}

static bool arc_file_exists(const ArcStr *path) { return !arc_has_nul(path) && access(path->data, F_OK) == 0; }

static ArcStr *arc_getenv(const ArcStr *name) {
    if (arc_has_nul(name)) return NULL;
    const char *v = getenv(name->data);
    return v ? arc_str_new(v, (int64_t)strlen(v)) : NULL;
}

static int64_t arc_time_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (int64_t)ts.tv_sec * 1000000000LL + ts.tv_nsec;
}
static int64_t arc_unix_time(void) { return (int64_t)time(NULL); }

static void arc_sleep_ms(int64_t ms) {
    if (ms <= 0) return;
    struct timespec ts = {(time_t)(ms / 1000), (long)(ms % 1000) * 1000000L};
    nanosleep(&ts, NULL);
}

static uint64_t arc_rng = 0x9E3779B97F4A7C15ULL;
static uint64_t arc_rand_next(void) {
    arc_rng ^= arc_rng << 13; arc_rng ^= arc_rng >> 7; arc_rng ^= arc_rng << 17;
    return arc_rng * 0x2545F4914F6CDD1DULL;
}
static void arc_seed(int64_t s) { arc_rng = arc_hash_u64((uint64_t)s) | 1; }

static void arc_init_random(void) {
    uint64_t k[3] = {0, 0, 0};
    FILE *f = fopen("/dev/urandom", "rb");
    size_t got = f ? fread(k, 1, sizeof k, f) : 0;
    if (f) fclose(f);
    if (got != sizeof k) {  /* fallback: still unpredictable enough across runs */
        uint64_t t = (uint64_t)arc_time_ns() ^ ((uint64_t)getpid() << 32) ^ (uint64_t)(uintptr_t)&k;
        k[0] = arc_hash_u64(t);
        k[1] = arc_hash_u64(k[0] ^ 0x9E3779B97F4A7C15ULL);
        k[2] = arc_hash_u64(k[1] ^ t);
    }
    arc_hash_k0 = k[0];
    arc_hash_k1 = k[1];
    arc_rng = k[2] | 1;
}
static int64_t arc_rand_int(int64_t lo, int64_t hi, ArcLoc loc) {
    if (lo > hi) arc_panic_at(loc, "rand_int(%lld, %lld): empty range", (long long)lo, (long long)hi);
    uint64_t span = (uint64_t)hi - (uint64_t)lo + 1;
    if (span == 0) return (int64_t)arc_rand_next();
    return lo + (int64_t)(arc_rand_next() % span);
}
static double arc_rand_float(void) { return (double)(arc_rand_next() >> 11) * (1.0 / 9007199254740992.0); }

/* run a program directly (no shell): arguments are never interpreted */
static int64_t arc_exec(const ArcStr *prog, const ArcArr *args) {
    if (arc_has_nul(prog)) return -1;
    ArcStr **av = (ArcStr **)args->data;
    for (int64_t i = 0; i < args->len; i++) if (arc_has_nul(av[i])) return -1;
    char **argv = (char **)arc_xcalloc(sizeof(char *) * (size_t)(args->len + 2));
    argv[0] = (char *)prog->data;
    for (int64_t i = 0; i < args->len; i++) argv[i + 1] = (char *)av[i]->data;
    fflush(stdout);
    fflush(stderr);
    pid_t pid = fork();
    if (pid == 0) {
        execvp(prog->data, argv);
        _exit(127);
    }
    free(argv);
    if (pid < 0) return -1;
    int st = 0;
    if (waitpid(pid, &st, 0) < 0) return -1;
    if (WIFEXITED(st)) return WEXITSTATUS(st);
    return WIFSIGNALED(st) ? 128 + WTERMSIG(st) : -1;
}

/* `![str]`: sorted entry names (without . and ..), or an error */
static void *arc_list_dir(const ArcStr *path) {
    if (arc_has_nul(path)) return arc_err_fmt("invalid path", path, 0);
    DIR *d = opendir(path->data);
    if (!d) return arc_err_fmt("cannot list directory", path, errno);
    arc_gc_inhibit++;
    ArcArr *r = arc_arr_new(&arc_td_arr_str, 0, 8);
    struct dirent *e;
    while ((e = readdir(d)) != NULL) {
        if (!strcmp(e->d_name, ".") || !strcmp(e->d_name, "..")) continue;
        ArcStr *name = arc_str_new(e->d_name, (int64_t)strlen(e->d_name));
        ARC_PUSH(ArcStr *, r, name);
    }
    closedir(d);
    arc_arr_sort(r);
    arc_gc_inhibit--;
    return r;
}

static ArcErr *arc_make_dir(const ArcStr *path) {
    if (arc_has_nul(path)) return arc_err_fmt("invalid path", path, 0);
    if (mkdir(path->data, 0777) != 0) return arc_err_fmt("cannot create directory", path, errno);
    return NULL;
}

static ArcErr *arc_remove_file(const ArcStr *path) {
    if (arc_has_nul(path)) return arc_err_fmt("invalid path", path, 0);
    if (remove(path->data) != 0) return arc_err_fmt("cannot remove", path, errno);
    return NULL;
}

static int64_t arc_run_cmd(const ArcStr *cmd) {
    if (arc_has_nul(cmd)) return -1;
    fflush(stdout);
    int st = system(cmd->data);
    if (st == -1) return -1;
    return WIFEXITED(st) ? WEXITSTATUS(st) : 128;
}

/* ------------------------------------------------------- stack overflow -- */

static void arc_segv_handler(int sig, siginfo_t *si, void *ctx) {
    (void)ctx;
    char *addr = (char *)si->si_addr;
    const char *msg;
    if (arc_stack_base && arc_stack_limit && addr <= arc_stack_base + 65536 &&
        addr >= arc_stack_base - arc_stack_limit - (1 << 20))
        msg = "\nfatal: stack overflow (is there unbounded recursion?)\n";
    else
        msg = sig == SIGBUS ? "\nfatal: bus error (internal runtime error)\n"
                            : "\nfatal: segmentation fault (internal runtime error)\n";
    ssize_t w = write(2, msg, strlen(msg));
    (void)w;
    _exit(134);
}

static void arc_install_signals(void) {
    static char altstack[65536];
    stack_t ss;
    ss.ss_sp = altstack;
    ss.ss_size = sizeof altstack;
    ss.ss_flags = 0;
    sigaltstack(&ss, NULL);
    struct sigaction sa;
    memset(&sa, 0, sizeof sa);
    sa.sa_sigaction = arc_segv_handler;
    sa.sa_flags = SA_SIGINFO | SA_ONSTACK;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGSEGV, &sa, NULL);
    sigaction(SIGBUS, &sa, NULL);
}

/* ---------------------------------------------------------------- init -- */

static void arc_runtime_init(int argc, char **argv, const char *const *files) {
    char here;
    arc_stack_base = &here;
    struct rlimit rl;
    if (getrlimit(RLIMIT_STACK, &rl) == 0 && rl.rlim_cur != RLIM_INFINITY) arc_stack_limit = (size_t)rl.rlim_cur;
    else arc_stack_limit = (size_t)8 << 20;
    arc_file_names = files;
    arc_init_classes();
    arc_install_signals();
    const char *st = getenv("ARC_GC_STRESS");
    arc_gc_stress = st && *st && *st != '0';
    arc_init_random();
    arc_add_groot((void **)&arc_args_arr);
    arc_args_arr = arc_arr_new(&arc_td_arr_str, 0, argc);
    arc_gc_inhibit++;
    for (int i = 0; i < argc; i++) {
        ArcStr *s = arc_str_new(argv[i], (int64_t)strlen(argv[i]));
        ARC_PUSH(ArcStr *, arc_args_arr, s);
    }
    arc_gc_inhibit--;
}

/* --------------------------------------------------------- test runner -- */

typedef struct { const char *name; void (*fn)(void); } ArcTest;

static int arc_run_tests(const ArcTest *tests, int n, const char *filter) {
    int passed = 0, failed = 0, filtered = 0;
    const char **failures = (const char **)arc_xcalloc(sizeof(char *) * (size_t)(n + 1));
    char **msgs = (char **)arc_xcalloc(sizeof(char *) * (size_t)(n + 1));
    int run = 0;
    int color = isatty(1) && !getenv("NO_COLOR");
    const char *OK = color ? "\x1b[32mok\x1b[0m" : "ok";
    const char *FAILED = color ? "\x1b[31mFAILED\x1b[0m" : "FAILED";
    for (int i = 0; i < n; i++) if (!filter || strstr(tests[i].name, filter)) run++;
    printf("\nrunning %d test%s\n", run, run == 1 ? "" : "s");
    for (int i = 0; i < n; i++) {
        if (filter && !strstr(tests[i].name, filter)) { filtered++; continue; }
        jmp_buf jb;
        ArcFrame *saved = arc_ftop;
        printf("test %s ... ", tests[i].name);
        fflush(stdout);
        arc_test_jmp = &jb;
        if (setjmp(jb) == 0) {
            tests[i].fn();
            arc_test_jmp = NULL;
            printf("%s\n", OK);
            passed++;
        } else {
            arc_test_jmp = NULL;
            arc_ftop = saved;
            arc_gc_inhibit = 0;
            arc_pb.len = 0;
            printf("%s\n", FAILED);
            failures[failed] = tests[i].name;
            msgs[failed] = strdup(arc_test_msg);
            failed++;
        }
    }
    if (failed) {
        printf("\nfailures:\n");
        for (int i = 0; i < failed; i++) printf("\n---- %s ----\n%s\n", failures[i], msgs[i]);
        printf("\nfailures:\n");
        for (int i = 0; i < failed; i++) printf("    %s\n", failures[i]);
    }
    printf("\ntest result: %s. %d passed; %d failed; %d filtered out\n\n",
           failed ? FAILED : OK, passed, failed, filtered);
    fflush(stdout);
    return failed ? 101 : 0;
}

/* ===================== end of runtime — generated code follows ========== */
