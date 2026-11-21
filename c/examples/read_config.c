#include "cine.h"

#include <inttypes.h>
#include <stdio.h>

int main(void) {
    CineConfig *config = NULL;
    CineProperties properties = {0};
    CineError error = {0};
    CineType retry_delays_type = cine_type_array(&CINE_TYPE_INT);
    CineType tags_type = cine_type_array(&CINE_TYPE_STRING);
    CineType pair_type = cine_type_fixed_array(&CINE_TYPE_INT, 2);
    CineValue app_name_default = cine_value_string("unnamed");
    CineValue debug_default = cine_value_bool(false);
    CineValue port_default = cine_value_int(3000);
    CineValue retry_delays_default = cine_value_array(NULL, 0);
    CineValue tags_default = cine_value_array(NULL, 0);
    CineValue pair_items[] = {cine_value_int(0), cine_value_int(0)};
    CineValue pair_default = cine_value_array(pair_items, 2);

    if (!cine_config_new("examples/config.json", CINE_REJECT_UNKNOWN, &config,
                         &error)) {
        fprintf(stderr, "Could not create configuration: %s\n", error.message);
        return 1;
    }

    if (!cine_config_prop(config, "app_name", &CINE_TYPE_STRING, &app_name_default,
                          &error) ||
        !cine_config_prop(config, "debug", &CINE_TYPE_BOOL, &debug_default, &error) ||
        !cine_config_prop(config, "http_port", &CINE_TYPE_INT, &port_default, &error) ||
        !cine_config_prop(config, "retry_delays", &retry_delays_type,
                          &retry_delays_default, &error) ||
        !cine_config_prop(config, "tags", &tags_type, &tags_default, &error) ||
        !cine_config_prop(config, "pair", &pair_type, &pair_default, &error) ||
        !cine_config_parse(config, &properties, &error)) {
        fprintf(stderr, "Could not load configuration: %s\n", error.message);
        cine_properties_free(&properties);
        cine_config_free(config);
        return 1;
    }

    const CineValue *name = cine_properties_get(&properties, "app_name");
    const CineValue *debug = cine_properties_get(&properties, "debug");
    const CineValue *port = cine_properties_get(&properties, "http_port");
    const CineValue *delays = cine_properties_get(&properties, "retry_delays");

    printf("%.*s (debug=%s, port=%" PRId64 ")\n", (int)name->as.string.length,
           name->as.string.data, debug->as.boolean ? "on" : "off", port->as.integer);
    printf("Retry delays:");
    for (size_t i = 0; i < delays->as.array.length; ++i) {
        printf(" %" PRId64, delays->as.array.items[i].as.integer);
    }
    putchar('\n');

    cine_properties_free(&properties);
    cine_config_free(config);
    return 0;
}
