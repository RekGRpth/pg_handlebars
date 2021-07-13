#include <handlebars/handlebars_json.h>
#include <json-c/json.h>
#include "pg_handlebars_json.h"

void handlebars_value_init_json_string_length(struct handlebars_context *ctx, struct handlebars_value * value, const char * json, size_t length) {
    enum json_tokener_error error;
    struct json_object *root;
    struct json_tokener *tok;
    if (!(tok = json_tokener_new())) handlebars_throw(ctx, HANDLEBARS_ERROR, "!json_tokener_new");
    do root = json_tokener_parse_ex(tok, json, length); while ((error = json_tokener_get_error(tok)) == json_tokener_continue);
    if (error != json_tokener_success) handlebars_throw(ctx, HANDLEBARS_ERROR, "!json_tokener_parse_ex and %s", json_tokener_error_desc(error));
    if (json_tokener_get_parse_end(tok) < length) handlebars_throw(ctx, HANDLEBARS_ERROR, "json_tokener_get_parse_end < %li", length);
    json_tokener_free(tok);
    handlebars_value_init_json_object(ctx, value, root);
    json_object_put(root);
}
