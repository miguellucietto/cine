#ifndef CINE_H
#define CINE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    CINE_NULL,
    CINE_BOOL,
    CINE_INT,
    CINE_FLOAT64,
    CINE_STRING,
    CINE_ARRAY,
    CINE_OBJECT,
    CINE_ANY,
    CINE_NUMBER
} CineKind;

typedef enum {
    CINE_IGNORE_UNKNOWN,
    CINE_REJECT_UNKNOWN,
    CINE_PANIC_UNKNOWN
} CineUnknownPolicy;

typedef enum {
    CINE_OK,
    CINE_ERROR_ARGUMENT,
    CINE_ERROR_MEMORY,
    CINE_ERROR_IO,
    CINE_ERROR_JSON,
    CINE_ERROR_TOP_LEVEL,
    CINE_ERROR_UNKNOWN_PROPERTY,
    CINE_ERROR_TYPE,
    CINE_ERROR_NUMBER_RANGE
} CineErrorCode;

typedef struct {
    CineErrorCode code;
    size_t line;
    size_t column;
    char message[512];
} CineError;

typedef struct CineType {
    CineKind kind;
    const struct CineType *element_type;
    bool fixed_length;
    size_t length;
} CineType;

typedef struct {
    char *data;
    size_t length;
} CineString;

typedef struct CineValue CineValue;
typedef struct CineObjectEntry CineObjectEntry;

struct CineValue {
    CineKind kind;
    bool owns_data;
    union {
        bool boolean;
        int64_t integer;
        double float64;
        CineString string;
        CineString number;
        struct {
            CineValue *items;
            size_t length;
        } array;
        struct {
            CineObjectEntry *items;
            size_t length;
        } object;
    } as;
};

struct CineObjectEntry {
    CineString key;
    CineValue value;
};

typedef struct CineConfig CineConfig;

/*
 * Parse result container. Initialize to {0} before use. The item storage is
 * private; use cine_properties_get to access values and cine_properties_free
 * to release the result.
 */
typedef struct {
    void *items;
    size_t length;
} CineProperties;

extern const CineType CINE_TYPE_NULL;
extern const CineType CINE_TYPE_BOOL;
extern const CineType CINE_TYPE_INT;
extern const CineType CINE_TYPE_FLOAT64;
extern const CineType CINE_TYPE_STRING;
extern const CineType CINE_TYPE_OBJECT;
extern const CineType CINE_TYPE_ANY;

/* Construct dynamic or exact-length array schemas. */
CineType cine_type_array(const CineType *element_type);
CineType cine_type_fixed_array(const CineType *element_type, size_t length);

/*
 * Value constructors create borrowed values. cine_config_prop clones the
 * supplied default, so caller-owned strings and arrays may be released after
 * registration.
 */
CineValue cine_value_null(void);
CineValue cine_value_bool(bool value);
CineValue cine_value_int(int64_t value);
CineValue cine_value_float64(double value);
CineValue cine_value_string(const char *value);
CineValue cine_value_string_n(const char *value, size_t length);
CineValue cine_value_array(const CineValue *items, size_t length);
CineValue cine_value_object(const CineObjectEntry *items, size_t length);
void cine_value_free(CineValue *value);

bool cine_config_new(const char *file_path, CineUnknownPolicy policy, CineConfig **out,
                     CineError *error);
void cine_config_free(CineConfig *config);
bool cine_config_prop(CineConfig *config, const char *name, const CineType *type,
                      const CineValue *default_value, CineError *error);

/* Parse into a new result. On failure, *out is not modified. */
bool cine_config_parse(const CineConfig *config, CineProperties *out, CineError *error);
bool cine_config_parse_file(const CineConfig *config, const char *file_path,
                            CineProperties *out, CineError *error);
void cine_properties_free(CineProperties *properties);
/* The returned pointer is borrowed and remains valid until properties_free. */
const CineValue *cine_properties_get(const CineProperties *properties,
                                     const char *name);

#ifdef __cplusplus
}
#endif

#endif
