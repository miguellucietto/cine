#include "cine.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "%s:%d: check failed: %s\n", __FILE__, __LINE__,           \
                    #condition);                                                       \
            return 1;                                                                  \
        }                                                                              \
    } while (0)

static bool register_properties(CineConfig *config, CineType *delays_type,
                                CineType *tags_type, CineType *pair_type,
                                CineError *error) {
    CineValue name = cine_value_string("default name");
    CineValue debug = cine_value_bool(false);
    CineValue port = cine_value_int(3000);
    CineValue delays = cine_value_array(NULL, 0);
    CineValue tags = cine_value_array(NULL, 0);
    CineValue pair_items[] = {cine_value_int(0), cine_value_int(0)};
    CineValue pair = cine_value_array(pair_items, 2);

    return cine_config_prop(config, "app_name", &CINE_TYPE_STRING, &name, error) &&
           cine_config_prop(config, "debug", &CINE_TYPE_BOOL, &debug, error) &&
           cine_config_prop(config, "http_port", &CINE_TYPE_INT, &port, error) &&
           cine_config_prop(config, "retry_delays", delays_type, &delays, error) &&
           cine_config_prop(config, "tags", tags_type, &tags, error) &&
           cine_config_prop(config, "pair", pair_type, &pair, error);
}

static int test_valid_configuration(void) {
    CineConfig *config = NULL;
    CineProperties properties = {0};
    CineError error = {0};
    CineType delays_type = cine_type_array(&CINE_TYPE_INT);
    CineType tags_type = cine_type_array(&CINE_TYPE_STRING);
    CineType pair_type = cine_type_fixed_array(&CINE_TYPE_INT, 2);

    CHECK(cine_config_new("unused.json", CINE_REJECT_UNKNOWN, &config, &error));
    CHECK(register_properties(config, &delays_type, &tags_type, &pair_type, &error));
    CHECK(cine_config_parse_file(config, "tests/fixtures/valid.json", &properties,
                                 &error));

    const CineValue *name = cine_properties_get(&properties, "app_name");
    const CineValue *debug = cine_properties_get(&properties, "debug");
    const CineValue *port = cine_properties_get(&properties, "http_port");
    const CineValue *delays = cine_properties_get(&properties, "retry_delays");
    const CineValue *tags = cine_properties_get(&properties, "tags");
    const CineValue *pair = cine_properties_get(&properties, "pair");
    CHECK(name && name->kind == CINE_STRING);
    CHECK(name->as.string.length == strlen("Test application"));
    CHECK(memcmp(name->as.string.data, "Test application", name->as.string.length) ==
          0);
    CHECK(debug && debug->kind == CINE_BOOL && debug->as.boolean);
    CHECK(port && port->kind == CINE_INT && port->as.integer == 8443);
    CHECK(delays && delays->as.array.length == 3);
    CHECK(delays->as.array.items[2].as.integer == 8);
    CHECK(tags && tags->as.array.length == 2);
    CHECK(pair && pair->as.array.length == 2);
    CHECK(pair->as.array.items[0].as.integer == 11);
    CHECK(pair->as.array.items[1].as.integer == 22);

    cine_properties_free(&properties);
    cine_config_free(config);
    return 0;
}

static int test_defaults_and_ignore_unknown(void) {
    CineConfig *config = NULL;
    CineProperties properties = {0};
    CineError error = {0};
    CineType delays_type = cine_type_array(&CINE_TYPE_INT);
    CineType tags_type = cine_type_array(&CINE_TYPE_STRING);
    CineType pair_type = cine_type_fixed_array(&CINE_TYPE_INT, 2);

    CHECK(cine_config_new("unused.json", CINE_IGNORE_UNKNOWN, &config, &error));
    CHECK(register_properties(config, &delays_type, &tags_type, &pair_type, &error));
    CHECK(cine_config_parse_file(config, "tests/fixtures/unknown_property.json",
                                 &properties, &error));
    const CineValue *name = cine_properties_get(&properties, "app_name");
    const CineValue *port = cine_properties_get(&properties, "http_port");
    CHECK(name && name->as.string.length == strlen("Test application"));
    CHECK(port && port->as.integer == 3000);
    cine_properties_free(&properties);
    cine_config_free(config);
    return 0;
}

static int test_reject_unknown_and_invalid_files(void) {
    CineConfig *config = NULL;
    CineProperties properties = {0};
    CineError error = {0};
    CineType delays_type = cine_type_array(&CINE_TYPE_INT);
    CineType tags_type = cine_type_array(&CINE_TYPE_STRING);
    CineType pair_type = cine_type_fixed_array(&CINE_TYPE_INT, 2);

    CHECK(cine_config_new("unused.json", CINE_REJECT_UNKNOWN, &config, &error));
    CHECK(register_properties(config, &delays_type, &tags_type, &pair_type, &error));
    CHECK(!cine_config_parse_file(config, "tests/fixtures/unknown_property.json",
                                  &properties, &error));
    CHECK(error.code == CINE_ERROR_UNKNOWN_PROPERTY);
    CHECK(!properties.items && properties.length == 0);

    memset(&error, 0, sizeof error);
    CHECK(!cine_config_parse_file(config, "tests/fixtures/wrong_type.json", &properties,
                                  &error));
    CHECK(error.code == CINE_ERROR_TYPE);

    memset(&error, 0, sizeof error);
    CHECK(!cine_config_parse_file(config, "tests/fixtures/malformed.json", &properties,
                                  &error));
    CHECK(error.code == CINE_ERROR_JSON);

    memset(&error, 0, sizeof error);
    CHECK(!cine_config_parse_file(config, "tests/fixtures/fixed_array_mismatch.json",
                                  &properties, &error));
    CHECK(error.code == CINE_ERROR_TYPE);

    cine_properties_free(&properties);
    cine_config_free(config);
    return 0;
}

static int test_exact_integer_conversion(void) {
    CineConfig *config = NULL;
    CineProperties properties = {0};
    CineError error = {0};
    CineValue default_value = cine_value_int(0);
    CHECK(cine_config_new("unused.json", CINE_REJECT_UNKNOWN, &config, &error));
    CHECK(cine_config_prop(config, "value", &CINE_TYPE_INT, &default_value, &error));
    CHECK(cine_config_parse_file(config, "tests/fixtures/integral_decimal.json",
                                 &properties, &error));
    const CineValue *value = cine_properties_get(&properties, "value");
    CHECK(value && value->as.integer == INT64_C(1200));
    cine_properties_free(&properties);

    CHECK(!cine_config_parse_file(config, "tests/fixtures/fractional_integer.json",
                                  &properties, &error));
    CHECK(error.code == CINE_ERROR_NUMBER_RANGE);
    memset(&error, 0, sizeof error);
    CHECK(!cine_config_parse_file(config, "tests/fixtures/integer_overflow.json",
                                  &properties, &error));
    CHECK(error.code == CINE_ERROR_NUMBER_RANGE);
    cine_config_free(config);
    return 0;
}

int main(void) {
    if (test_valid_configuration() || test_defaults_and_ignore_unknown() ||
        test_reject_unknown_and_invalid_files() || test_exact_integer_conversion()) {
        return EXIT_FAILURE;
    }
    puts("All cine C tests passed.");
    return EXIT_SUCCESS;
}
