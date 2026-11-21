#define _POSIX_C_SOURCE 200809L
#include "cine.h"

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

const CineType CINE_TYPE_NULL = {CINE_NULL, NULL, false, 0};
const CineType CINE_TYPE_BOOL = {CINE_BOOL, NULL, false, 0};
const CineType CINE_TYPE_INT = {CINE_INT, NULL, false, 0};
const CineType CINE_TYPE_FLOAT64 = {CINE_FLOAT64, NULL, false, 0};
const CineType CINE_TYPE_STRING = {CINE_STRING, NULL, false, 0};
const CineType CINE_TYPE_OBJECT = {CINE_OBJECT, NULL, false, 0};
const CineType CINE_TYPE_ANY = {CINE_ANY, NULL, false, 0};

typedef struct {
    char *name;
    CineType type;
    CineValue value;
} Property;
struct CineConfig {
    char *path;
    CineUnknownPolicy policy;
    Property *props;
    size_t count;
};
typedef struct {
    const char *s;
    size_t n, p, line, col;
    CineError *err;
} Parser;

static void error_set(CineError *e, CineErrorCode c, const char *m) {
    if (!e)
        return;
    e->code = c;
    snprintf(e->message, sizeof e->message, "%s", m ? m : "");
}
static void *memdup0(const void *p, size_t n) {
    char *q = malloc(n + 1);
    if (q) {
        if (n)
            memcpy(q, p, n);
        q[n] = 0;
    }
    return q;
}
CineType cine_type_array(const CineType *t) {
    return (CineType){CINE_ARRAY, t, false, 0};
}
CineType cine_type_fixed_array(const CineType *t, size_t n) {
    return (CineType){CINE_ARRAY, t, true, n};
}
CineValue cine_value_null(void) { return (CineValue){.kind = CINE_NULL}; }
CineValue cine_value_bool(bool b) {
    CineValue v = {.kind = CINE_BOOL};
    v.as.boolean = b;
    return v;
}
CineValue cine_value_int(int64_t n) {
    CineValue v = {.kind = CINE_INT};
    v.as.integer = n;
    return v;
}
CineValue cine_value_float64(double n) {
    CineValue v = {.kind = CINE_FLOAT64};
    v.as.float64 = n;
    return v;
}
CineValue cine_value_string_n(const char *s, size_t n) {
    CineValue v = {.kind = CINE_STRING};
    v.as.string = (CineString){(char *)s, n};
    return v;
}
CineValue cine_value_string(const char *s) {
    return cine_value_string_n(s ? s : "", s ? strlen(s) : 0);
}
CineValue cine_value_array(const CineValue *a, size_t n) {
    CineValue v = {.kind = CINE_ARRAY};
    v.as.array.items = (CineValue *)a;
    v.as.array.length = n;
    return v;
}
CineValue cine_value_object(const CineObjectEntry *a, size_t n) {
    CineValue v = {.kind = CINE_OBJECT};
    v.as.object.items = (CineObjectEntry *)a;
    v.as.object.length = n;
    return v;
}
void cine_value_free(CineValue *v) {
    if (!v || !v->owns_data)
        return;
    if (v->kind == CINE_STRING)
        free(v->as.string.data);
    else if (v->kind == CINE_NUMBER)
        free(v->as.number.data);
    else if (v->kind == CINE_ARRAY) {
        for (size_t i = 0; i < v->as.array.length; i++)
            cine_value_free(&v->as.array.items[i]);
        free(v->as.array.items);
    } else if (v->kind == CINE_OBJECT) {
        for (size_t i = 0; i < v->as.object.length; i++) {
            free(v->as.object.items[i].key.data);
            cine_value_free(&v->as.object.items[i].value);
        }
        free(v->as.object.items);
    }
    memset(v, 0, sizeof *v);
}
static bool clone_value(const CineValue *a, CineValue *b) {
    *b = *a;
    b->owns_data = false;
    if (a->kind == CINE_STRING || a->kind == CINE_NUMBER) {
        CineString s = a->kind == CINE_STRING ? a->as.string : a->as.number;
        char *p = memdup0(s.data, s.length);
        if (!p)
            return false;
        if (a->kind == CINE_STRING)
            b->as.string.data = p;
        else
            b->as.number.data = p;
        b->owns_data = true;
    } else if (a->kind == CINE_ARRAY) {
        b->as.array.items = calloc(a->as.array.length, sizeof(CineValue));
        if (a->as.array.length && !b->as.array.items)
            return false;
        b->owns_data = true;
        for (size_t i = 0; i < a->as.array.length; i++) {
            if (!clone_value(&a->as.array.items[i], &b->as.array.items[i])) {
                b->as.array.length = i;
                cine_value_free(b);
                return false;
            }
        }
    } else if (a->kind == CINE_OBJECT) {
        b->as.object.items = calloc(a->as.object.length, sizeof(CineObjectEntry));
        if (a->as.object.length && !b->as.object.items)
            return false;
        b->owns_data = true;
        for (size_t i = 0; i < a->as.object.length; i++) {
            CineString k = a->as.object.items[i].key;
            b->as.object.items[i].key =
                (CineString){memdup0(k.data, k.length), k.length};
            if (!b->as.object.items[i].key.data ||
                !clone_value(&a->as.object.items[i].value,
                             &b->as.object.items[i].value)) {
                b->as.object.length = i + 1;
                cine_value_free(b);
                return false;
            }
        }
    }
    return true;
}
static void advance(Parser *p) {
    if (p->p < p->n) {
        if (p->s[p->p++] == '\n') {
            p->line++;
            p->col = 1;
        } else
            p->col++;
    }
}
static void ws(Parser *p) {
    while (p->p < p->n && isspace((unsigned char)p->s[p->p]))
        advance(p);
}
static bool fail(Parser *p, const char *m) {
    if (p->err) {
        p->err->line = p->line;
        p->err->column = p->col;
    }
    error_set(p->err, CINE_ERROR_JSON, m);
    return false;
}
static bool utf8(char **out, size_t *len, unsigned cp) {
    unsigned char b[4];
    size_t n;
    if (cp < 0x80) {
        b[0] = cp;
        n = 1;
    } else if (cp < 0x800) {
        b[0] = 0xc0 | (cp >> 6);
        b[1] = 0x80 | (cp & 63);
        n = 2;
    } else if (cp < 0x10000) {
        b[0] = 0xe0 | (cp >> 12);
        b[1] = 0x80 | ((cp >> 6) & 63);
        b[2] = 0x80 | (cp & 63);
        n = 3;
    } else {
        b[0] = 0xf0 | (cp >> 18);
        b[1] = 0x80 | ((cp >> 12) & 63);
        b[2] = 0x80 | ((cp >> 6) & 63);
        b[3] = 0x80 | (cp & 63);
        n = 4;
    }
    char *q = realloc(*out, *len + n + 1);
    if (!q)
        return false;
    memcpy(q + *len, b, n);
    *len += n;
    q[*len] = 0;
    *out = q;
    return true;
}
static int hex4(Parser *p) {
    int x = 0;
    for (int i = 0; i < 4; i++) {
        if (p->p >= p->n)
            return -1;
        char c = p->s[p->p];
        advance(p);
        x <<= 4;
        if (c >= '0' && c <= '9')
            x |= c - '0';
        else if (c >= 'a' && c <= 'f')
            x |= c - 'a' + 10;
        else if (c >= 'A' && c <= 'F')
            x |= c - 'A' + 10;
        else
            return -1;
    }
    return x;
}
static bool parse_string(Parser *p, CineString *out) {
    if (p->p >= p->n || p->s[p->p] != '"')
        return fail(p, "expected string");
    advance(p);
    char *b = NULL;
    size_t n = 0;
    while (p->p < p->n && p->s[p->p] != '"') {
        unsigned char c = p->s[p->p];
        advance(p);
        if (c < 32) {
            free(b);
            return fail(p, "control character in string");
        }
        if (c == '\\') {
            if (p->p >= p->n) {
                free(b);
                return fail(p, "incomplete escape");
            }
            char e = p->s[p->p];
            advance(p);
            switch (e) {
            case '"':
            case '\\':
            case '/':
                c = e;
                break;
            case 'b':
                c = '\b';
                break;
            case 'f':
                c = '\f';
                break;
            case 'n':
                c = '\n';
                break;
            case 'r':
                c = '\r';
                break;
            case 't':
                c = '\t';
                break;
            case 'u': {
                int cp = hex4(p);
                if (cp < 0) {
                    free(b);
                    return fail(p, "invalid unicode escape");
                }
                if (cp >= 0xd800 && cp <= 0xdbff) {
                    if (p->p + 2 > p->n || p->s[p->p] != '\\' ||
                        p->s[p->p + 1] != 'u') {
                        free(b);
                        return fail(p, "invalid surrogate pair");
                    }
                    advance(p);
                    advance(p);
                    int lo = hex4(p);
                    if (lo < 0xdc00 || lo > 0xdfff) {
                        free(b);
                        return fail(p, "invalid surrogate pair");
                    }
                    cp = 0x10000 + ((cp - 0xd800) << 10) + (lo - 0xdc00);
                } else if (cp >= 0xdc00 && cp <= 0xdfff) {
                    free(b);
                    return fail(p, "invalid surrogate");
                }
                if (!utf8(&b, &n, (unsigned)cp)) {
                    free(b);
                    return false;
                }
                continue;
            }
            default:
                free(b);
                return fail(p, "invalid escape");
            }
        }
        char *q = realloc(b, n + 2);
        if (!q) {
            free(b);
            return false;
        }
        b = q;
        b[n++] = (char)c;
        b[n] = 0;
    }
    if (p->p >= p->n) {
        free(b);
        return fail(p, "unterminated string");
    }
    advance(p);
    if (!b)
        b = memdup0("", 0);
    if (!b)
        return false;
    *out = (CineString){b, n};
    return true;
}
static bool parse_value(Parser *, CineValue *);
static bool parse_array(Parser *p, CineValue *v) {
    advance(p);
    ws(p);
    CineValue *a = NULL;
    size_t n = 0;
    if (p->p < p->n && p->s[p->p] == ']') {
        advance(p);
        v->kind = CINE_ARRAY;
        v->owns_data = true;
        return true;
    }
    for (;;) {
        CineValue x = {0};
        if (!parse_value(p, &x))
            goto bad;
        CineValue *q = realloc(a, (n + 1) * sizeof x);
        if (!q) {
            cine_value_free(&x);
            goto bad;
        }
        a = q;
        a[n++] = x;
        ws(p);
        if (p->p >= p->n)
            break;
        if (p->s[p->p] == ']') {
            advance(p);
            v->kind = CINE_ARRAY;
            v->owns_data = true;
            v->as.array.items = a;
            v->as.array.length = n;
            return true;
        }
        if (p->s[p->p] != ',')
            break;
        advance(p);
        ws(p);
    }
    fail(p, "invalid array");
bad:
    for (size_t i = 0; i < n; i++)
        cine_value_free(&a[i]);
    free(a);
    return false;
}
static bool parse_object(Parser *p, CineValue *v) {
    advance(p);
    ws(p);
    CineObjectEntry *a = NULL;
    size_t n = 0;
    if (p->p < p->n && p->s[p->p] == '}') {
        advance(p);
        v->kind = CINE_OBJECT;
        v->owns_data = true;
        return true;
    }
    for (;;) {
        CineString k = {0};
        if (!parse_string(p, &k))
            goto bad;
        ws(p);
        if (p->p >= p->n || p->s[p->p] != ':') {
            free(k.data);
            break;
        }
        advance(p);
        ws(p);
        CineValue x = {0};
        if (!parse_value(p, &x)) {
            free(k.data);
            goto bad;
        }
        CineObjectEntry *q = realloc(a, (n + 1) * sizeof(CineObjectEntry));
        if (!q) {
            free(k.data);
            cine_value_free(&x);
            goto bad;
        }
        a = q;
        a[n++] = (CineObjectEntry){k, x};
        ws(p);
        if (p->p >= p->n)
            break;
        if (p->s[p->p] == '}') {
            advance(p);
            v->kind = CINE_OBJECT;
            v->owns_data = true;
            v->as.object.items = a;
            v->as.object.length = n;
            return true;
        }
        if (p->s[p->p] != ',')
            break;
        advance(p);
        ws(p);
    }
    fail(p, "invalid object");
bad:
    for (size_t i = 0; i < n; i++) {
        free(a[i].key.data);
        cine_value_free(&a[i].value);
    }
    free(a);
    return false;
}
static bool parse_value(Parser *p, CineValue *v) {
    ws(p);
    if (p->p >= p->n)
        return fail(p, "unexpected end of input");
    char c = p->s[p->p];
    if (c == '{')
        return parse_object(p, v);
    if (c == '[')
        return parse_array(p, v);
    if (c == '"') {
        v->kind = CINE_STRING;
        v->owns_data = true;
        return parse_string(p, &v->as.string);
    }
    if (!strncmp(p->s + p->p, "true", 4)) {
        for (int i = 0; i < 4; i++)
            advance(p);
        *v = cine_value_bool(true);
        return true;
    }
    if (!strncmp(p->s + p->p, "false", 5)) {
        for (int i = 0; i < 5; i++)
            advance(p);
        *v = cine_value_bool(false);
        return true;
    }
    if (!strncmp(p->s + p->p, "null", 4)) {
        for (int i = 0; i < 4; i++)
            advance(p);
        *v = cine_value_null();
        return true;
    }
    size_t start = p->p;
    if (c == '-')
        advance(p);
    if (p->p >= p->n)
        return fail(p, "invalid number");
    if (p->s[p->p] == '0')
        advance(p);
    else if (isdigit((unsigned char)p->s[p->p])) {
        while (p->p < p->n && isdigit((unsigned char)p->s[p->p]))
            advance(p);
    } else
        return fail(p, "invalid value");
    if (p->p < p->n && p->s[p->p] == '.') {
        advance(p);
        if (p->p >= p->n || !isdigit((unsigned char)p->s[p->p]))
            return fail(p, "invalid number");
        while (p->p < p->n && isdigit((unsigned char)p->s[p->p]))
            advance(p);
    }
    if (p->p < p->n && (p->s[p->p] == 'e' || p->s[p->p] == 'E')) {
        advance(p);
        if (p->p < p->n && (p->s[p->p] == '+' || p->s[p->p] == '-'))
            advance(p);
        if (p->p >= p->n || !isdigit((unsigned char)p->s[p->p]))
            return fail(p, "invalid number");
        while (p->p < p->n && isdigit((unsigned char)p->s[p->p]))
            advance(p);
    }
    v->kind = CINE_NUMBER;
    v->owns_data = true;
    v->as.number = (CineString){memdup0(p->s + start, p->p - start), p->p - start};
    return v->as.number.data != NULL;
}

static bool number_int(CineString s, int64_t *out) {
    char *z = memdup0(s.data, s.length);
    if (!z)
        return false;
    char *p = z;
    bool neg = *p == '-';
    if (neg)
        p++;
    char digits[512];
    size_t nd = 0, frac = 0;
    while (isdigit((unsigned char)*p)) {
        if (nd < sizeof digits)
            digits[nd++] = *p;
        p++;
    }
    if (*p == '.') {
        p++;
        while (isdigit((unsigned char)*p)) {
            if (nd < sizeof digits)
                digits[nd++] = *p;
            frac++;
            p++;
        }
    }
    int exp = 0;
    if (*p == 'e' || *p == 'E') {
        p++;
        bool eneg = *p == '-';
        if (*p == '+' || *p == '-')
            p++;
        while (isdigit((unsigned char)*p)) {
            if (exp < 100000)
                exp = exp * 10 + (*p - '0');
            p++;
        }
        if (eneg)
            exp = -exp;
    }
    free(z);
    if (nd >= sizeof digits)
        return false;
    digits[nd] = 0;
    long scale = (long)exp - (long)frac;
    if (scale < 0) {
        long cut = -scale;
        if (cut > (long)nd)
            return false;
        while (cut-- > 0) {
            if (digits[--nd] != '0')
                return false;
        }
        digits[nd] = 0;
    } else {
        if (scale > 19 || nd + (size_t)scale > 19)
            return false;
        while (scale-- > 0)
            digits[nd++] = '0';
        digits[nd] = 0;
    }
    size_t i = 0;
    while (i + 1 < nd && digits[i] == '0')
        i++;
    uint64_t limit = neg ? UINT64_C(9223372036854775808) : INT64_MAX, x = 0;
    for (; i < nd; i++) {
        unsigned d = (unsigned)(digits[i] - '0');
        if (x > (limit - d) / 10)
            return false;
        x = x * 10 + d;
    }
    *out = neg ? (x == UINT64_C(9223372036854775808) ? INT64_MIN : -(int64_t)x)
               : (int64_t)x;
    return true;
}
static bool convert(const CineType *t, const CineValue *in, CineValue *out,
                    CineError *e) {
    if (t->kind == CINE_ANY)
        return clone_value(in, out);
    if (t->kind == CINE_ARRAY) {
        if (in->kind == CINE_NULL && !t->fixed_length) {
            out->kind = CINE_ARRAY;
            out->owns_data = true;
            return true;
        }
        if (in->kind != CINE_ARRAY) {
            error_set(e, CINE_ERROR_TYPE, "value is not a JSON array");
            return false;
        }
        size_t n = in->as.array.length;
        if (t->fixed_length && n != t->length) {
            error_set(e, CINE_ERROR_TYPE,
                      "array length does not match fixed array type");
            return false;
        }
        out->kind = CINE_ARRAY;
        out->owns_data = true;
        out->as.array.length = n;
        out->as.array.items = calloc(n, sizeof(CineValue));
        if (n && !out->as.array.items) {
            error_set(e, CINE_ERROR_MEMORY, "out of memory");
            return false;
        }
        for (size_t i = 0; i < n; i++)
            if (!convert(t->element_type, &in->as.array.items[i],
                         &out->as.array.items[i], e)) {
                cine_value_free(out);
                return false;
            }
        return true;
    }
    if (t->kind == CINE_INT && in->kind == CINE_NUMBER) {
        out->kind = CINE_INT;
        if (!number_int(in->as.number, &out->as.integer)) {
            error_set(e, CINE_ERROR_NUMBER_RANGE,
                      "JSON number is not an in-range integer");
            return false;
        }
        return true;
    }
    if (t->kind == CINE_FLOAT64 && in->kind == CINE_NUMBER) {
        char *z = memdup0(in->as.number.data, in->as.number.length);
        if (!z)
            return false;
        errno = 0;
        char *end;
        double d = strtod(z, &end);
        bool ok = *end == 0 && errno != ERANGE;
        free(z);
        if (!ok) {
            error_set(e, CINE_ERROR_NUMBER_RANGE,
                      "JSON number cannot be represented as float64");
            return false;
        }
        out->kind = CINE_FLOAT64;
        out->as.float64 = d;
        return true;
    }
    if (t->kind != in->kind) {
        error_set(e, CINE_ERROR_TYPE, "JSON value does not match registered type");
        return false;
    }
    return clone_value(in, out);
}
static void type_copy(const CineType *t, CineType *out) {
    *out = *t;
    if (t->element_type) {
        out->element_type = malloc(sizeof(CineType));
        if (out->element_type) {
            type_copy(t->element_type, (CineType *)out->element_type);
        }
    }
}
static void type_free(CineType *t) {
    if (t->element_type) {
        type_free((CineType *)t->element_type);
        free((void *)t->element_type);
    }
}
bool cine_config_new(const char *path, CineUnknownPolicy policy, CineConfig **out,
                     CineError *e) {
    if (!path || !out || policy > CINE_PANIC_UNKNOWN) {
        error_set(e, CINE_ERROR_ARGUMENT, "invalid config arguments");
        return false;
    }
    CineConfig *c = calloc(1, sizeof *c);
    if (!c) {
        error_set(e, CINE_ERROR_MEMORY, "out of memory");
        return false;
    }
    c->path = strdup(path);
    if (!c->path) {
        free(c);
        return false;
    }
    c->policy = policy;
    *out = c;
    return true;
}
void cine_config_free(CineConfig *c) {
    if (!c)
        return;
    for (size_t i = 0; i < c->count; i++) {
        free(c->props[i].name);
        type_free(&c->props[i].type);
        cine_value_free(&c->props[i].value);
    }
    free(c->props);
    free(c->path);
    free(c);
}
bool cine_config_prop(CineConfig *c, const char *n, const CineType *t,
                      const CineValue *v, CineError *e) {
    if (!c || !n || !t || !v) {
        error_set(e, CINE_ERROR_ARGUMENT, "invalid property arguments");
        return false;
    }
    for (size_t i = 0; i < c->count; i++)
        if (!strcmp(c->props[i].name, n)) {
            CineValue x = {0};
            if (!clone_value(v, &x)) {
                error_set(e, CINE_ERROR_MEMORY, "out of memory");
                return false;
            }
            type_free(&c->props[i].type);
            cine_value_free(&c->props[i].value);
            type_copy(t, &c->props[i].type);
            c->props[i].value = x;
            return true;
        }
    Property *p = realloc(c->props, (c->count + 1) * sizeof *p);
    if (!p) {
        error_set(e, CINE_ERROR_MEMORY, "out of memory");
        return false;
    }
    c->props = p;
    Property *x = &c->props[c->count];
    memset(x, 0, sizeof *x);
    x->name = strdup(n);
    if (!x->name || !clone_value(v, &x->value)) {
        free(x->name);
        error_set(e, CINE_ERROR_MEMORY, "out of memory");
        return false;
    }
    type_copy(t, &x->type);
    c->count++;
    return true;
}
static int prop_cmp(const void *a, const void *b) {
    const Property *x = *(const Property **)a, *y = *(const Property **)b;
    return strcmp(x->name, y->name);
}
static bool parse_data(const CineConfig *c, const char *data, size_t n,
                       CineProperties *out, CineError *e) {
    Parser p = {data, n, 0, 1, 1, e};
    CineValue root = {0};
    if (!parse_value(&p, &root))
        return false;
    ws(&p);
    if (p.p < n) {
        cine_value_free(&root);
        error_set(e, CINE_ERROR_JSON,
                  "configuration must contain exactly one JSON value");
        return false;
    }
    if (root.kind != CINE_OBJECT) {
        cine_value_free(&root);
        error_set(e, CINE_ERROR_TOP_LEVEL, "top-level JSON value must be an object");
        return false;
    }
    CineProperties res = {0};
    Property **sorted = malloc(c->count * sizeof *sorted);
    if (c->count && !sorted) {
        cine_value_free(&root);
        return false;
    }
    for (size_t i = 0; i < c->count; i++)
        sorted[i] = &c->props[i];
    qsort(sorted, c->count, sizeof *sorted, prop_cmp);
    CineValue *values = calloc(c->count, sizeof *values);
    if (c->count && !values) {
        free(sorted);
        cine_value_free(&root);
        return false;
    }
    for (size_t i = 0; i < c->count; i++)
        if (!clone_value(&sorted[i]->value, &values[i]))
            goto memfail;
    for (size_t k = 0; k < root.as.object.length; k++) {
        CineObjectEntry *entry = &root.as.object.items[k];
        size_t i = 0;
        while (i < c->count && strcmp(sorted[i]->name, entry->key.data))
            i++;
        if (i == c->count) {
            if (c->policy == CINE_PANIC_UNKNOWN) {
                fprintf(stderr, "Unknown property: %s\n", entry->key.data);
                abort();
            }
            if (c->policy == CINE_REJECT_UNKNOWN) {
                error_set(e, CINE_ERROR_UNKNOWN_PROPERTY, "unknown JSON property");
                goto failure;
            }
            continue;
        }
        CineValue v = {0};
        if (!convert(&sorted[i]->type, &entry->value, &v, e))
            goto failure;
        cine_value_free(&values[i]);
        values[i] = v;
    }
    typedef struct {
        char *name;
        CineValue value;
    } ResultItem;
    ResultItem *items = calloc(c->count, sizeof *items);
    if (c->count && !items)
        goto memfail;
    for (size_t i = 0; i < c->count; i++) {
        items[i].name = strdup(sorted[i]->name);
        if (!items[i].name)
            goto memfail;
        items[i].value = values[i];
        memset(&values[i], 0, sizeof values[i]);
    }
    res.items = items;
    res.length = c->count;
    *out = res;
    free(values);
    free(sorted);
    cine_value_free(&root);
    return true;
memfail:
    error_set(e, CINE_ERROR_MEMORY, "out of memory");
failure:
    for (size_t i = 0; i < c->count; i++)
        cine_value_free(&values[i]);
    free(values);
    free(sorted);
    cine_value_free(&root);
    return false;
}
bool cine_config_parse_file(const CineConfig *c, const char *path, CineProperties *out,
                            CineError *e) {
    if (!c || !path || !out) {
        error_set(e, CINE_ERROR_ARGUMENT, "invalid parse arguments");
        return false;
    }
    FILE *f = fopen(path, "rb");
    if (!f) {
        error_set(e, CINE_ERROR_IO, strerror(errno));
        return false;
    }
    if (fseek(f, 0, SEEK_END)) {
        fclose(f);
        error_set(e, CINE_ERROR_IO, "failed to seek configuration file");
        return false;
    }
    long z = ftell(f);
    if (z < 0 || fseek(f, 0, SEEK_SET)) {
        fclose(f);
        error_set(e, CINE_ERROR_IO, "failed to read configuration file");
        return false;
    }
    char *b = malloc((size_t)z + 1);
    if (!b) {
        fclose(f);
        error_set(e, CINE_ERROR_MEMORY, "out of memory");
        return false;
    }
    size_t n = fread(b, 1, (size_t)z, f);
    bool ok = !ferror(f) && n == (size_t)z;
    fclose(f);
    if (!ok) {
        free(b);
        error_set(e, CINE_ERROR_IO, "failed to read configuration file");
        return false;
    }
    b[n] = 0;
    bool r = parse_data(c, b, n, out, e);
    free(b);
    return r;
}
bool cine_config_parse(const CineConfig *c, CineProperties *out, CineError *e) {
    return c ? cine_config_parse_file(c, c->path, out, e) : false;
}
void cine_properties_free(CineProperties *p) {
    if (!p || !p->items)
        return;
    typedef struct {
        char *name;
        CineValue value;
    } ResultItem;
    ResultItem *a = p->items;
    for (size_t i = 0; i < p->length; i++) {
        free(a[i].name);
        cine_value_free(&a[i].value);
    }
    free(a);
    p->items = NULL;
    p->length = 0;
}
const CineValue *cine_properties_get(const CineProperties *p, const char *n) {
    if (!p || !n)
        return NULL;
    struct ResultItem {
        char *name;
        CineValue value;
    };
    struct ResultItem *a = p->items;
    for (size_t i = 0; i < p->length; i++)
        if (!strcmp(a[i].name, n))
            return &a[i].value;
    return NULL;
}
