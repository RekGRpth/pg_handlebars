#ifndef PG_HANDLEBARS_JSON_H
#define PG_HANDLEBARS_JSON_H

HBS_EXTERN_C_START

struct handlebars_context;
struct handlebars_value;

/**
 * @brief Initialize a value from a JSON string with length
 * @param[in] ctx The handlebars context
 * @param[in] value The value to initialize
 * @param[in] json The JSON string
 * @param[in] length The JSON length
 */
void handlebars_value_init_json_string_length(
    struct handlebars_context * ctx,
    struct handlebars_value * value,
    const char * json,
    size_t length
) HBS_ATTR_NONNULL_ALL;

HBS_EXTERN_C_END

#endif /* PG_HANDLEBARS_JSON_H */
