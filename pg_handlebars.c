#include <postgres.h>

#include <handlebars/handlebars_compiler.h>
#include <handlebars/handlebars_delimiters.h>
#include <handlebars/handlebars_memory.h>
#include <handlebars/handlebars_opcode_serializer.h>
#include <handlebars/handlebars_parser.h>
#include <handlebars/handlebars_partial_loader.h>
#include <handlebars/handlebars_string.h>
#include <handlebars/handlebars_value.h>
#include <handlebars/handlebars_vm.h>
#include "pg_handlebars_json.h"
#include <talloc.h>
#include <utils/builtins.h>

#define EXTENSION(function) Datum (function)(PG_FUNCTION_ARGS); PG_FUNCTION_INFO_V1(function); Datum (function)(PG_FUNCTION_ARGS)

#define FORMAT_0(fmt, ...) "%s(%s:%d): %s", __func__, __FILE__, __LINE__, fmt
#define FORMAT_1(fmt, ...) "%s(%s:%d): " fmt,  __func__, __FILE__, __LINE__
#define GET_FORMAT(fmt, ...) GET_FORMAT_PRIVATE(fmt, 0, ##__VA_ARGS__, 1, \
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, \
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, \
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, \
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, \
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, \
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, \
    1, 1, 1, 1, 1, 1, 1, 1, 1, 0)
#define GET_FORMAT_PRIVATE(fmt, \
      _0,  _1,  _2,  _3,  _4,  _5,  _6,  _7,  _8,  _9, \
     _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, \
     _20, _21, _22, _23, _24, _25, _26, _27, _28, _29, \
     _30, _31, _32, _33, _34, _35, _36, _37, _38, _39, \
     _40, _41, _42, _43, _44, _45, _46, _47, _48, _49, \
     _50, _51, _52, _53, _54, _55, _56, _57, _58, _59, \
     _60, _61, _62, _63, _64, _65, _66, _67, _68, _69, \
     _70, format, ...) FORMAT_ ## format(fmt)

#define D1(fmt, ...) ereport(DEBUG1, (errmsg(GET_FORMAT(fmt, ##__VA_ARGS__), ##__VA_ARGS__)))
#define D2(fmt, ...) ereport(DEBUG2, (errmsg(GET_FORMAT(fmt, ##__VA_ARGS__), ##__VA_ARGS__)))
#define D3(fmt, ...) ereport(DEBUG3, (errmsg(GET_FORMAT(fmt, ##__VA_ARGS__), ##__VA_ARGS__)))
#define D4(fmt, ...) ereport(DEBUG4, (errmsg(GET_FORMAT(fmt, ##__VA_ARGS__), ##__VA_ARGS__)))
#define D5(fmt, ...) ereport(DEBUG5, (errmsg(GET_FORMAT(fmt, ##__VA_ARGS__), ##__VA_ARGS__)))
#define E(fmt, ...) ereport(ERROR, (errmsg(GET_FORMAT(fmt, ##__VA_ARGS__), ##__VA_ARGS__)))
#define F(fmt, ...) ereport(FATAL, (errmsg(GET_FORMAT(fmt, ##__VA_ARGS__), ##__VA_ARGS__)))
#define I(fmt, ...) ereport(INFO, (errmsg(GET_FORMAT(fmt, ##__VA_ARGS__), ##__VA_ARGS__)))
#define L(fmt, ...) ereport(LOG, (errmsg(GET_FORMAT(fmt, ##__VA_ARGS__), ##__VA_ARGS__)))
#define N(fmt, ...) ereport(NOTICE, (errmsg(GET_FORMAT(fmt, ##__VA_ARGS__), ##__VA_ARGS__)))
#define W(fmt, ...) ereport(WARNING, (errmsg(GET_FORMAT(fmt, ##__VA_ARGS__), ##__VA_ARGS__)))

PG_MODULE_MAGIC;

static bool convert_input = true;
static bool enable_partial_loader = true;
static long run_count = 1;
static struct handlebars_context *ctx = NULL;
static struct handlebars_string *partial_extension = NULL;
static struct handlebars_string *partial_path = NULL;
static TALLOC_CTX *root = NULL;
static unsigned long compiler_flags = handlebars_compiler_flag_none;

EXTENSION(pg_handlebars_compiler_flag_all) { compiler_flags |= handlebars_compiler_flag_all; PG_RETURN_NULL(); }
EXTENSION(pg_handlebars_compiler_flag_alternate_decorators) { compiler_flags |= handlebars_compiler_flag_alternate_decorators; PG_RETURN_NULL(); }
EXTENSION(pg_handlebars_compiler_flag_assume_objects) { compiler_flags |= handlebars_compiler_flag_assume_objects; PG_RETURN_NULL(); }
EXTENSION(pg_handlebars_compiler_flag_compat) { compiler_flags |= handlebars_compiler_flag_compat; PG_RETURN_NULL(); }
EXTENSION(pg_handlebars_compiler_flag_explicit_partial_context) { compiler_flags |= handlebars_compiler_flag_explicit_partial_context; PG_RETURN_NULL(); }
EXTENSION(pg_handlebars_compiler_flag_ignore_standalone) { compiler_flags |= handlebars_compiler_flag_ignore_standalone; PG_RETURN_NULL(); }
EXTENSION(pg_handlebars_compiler_flag_known_helpers_only) { compiler_flags |= handlebars_compiler_flag_known_helpers_only; PG_RETURN_NULL(); }
EXTENSION(pg_handlebars_compiler_flag_mustache_style_lambdas) { compiler_flags |= handlebars_compiler_flag_mustache_style_lambdas; PG_RETURN_NULL(); }
EXTENSION(pg_handlebars_compiler_flag_no_escape) { compiler_flags |= handlebars_compiler_flag_no_escape; PG_RETURN_NULL(); }
EXTENSION(pg_handlebars_compiler_flag_none) { compiler_flags = handlebars_compiler_flag_none; PG_RETURN_NULL(); }
EXTENSION(pg_handlebars_compiler_flag_prevent_indent) { compiler_flags |= handlebars_compiler_flag_prevent_indent; PG_RETURN_NULL(); }
EXTENSION(pg_handlebars_compiler_flag_strict) { compiler_flags |= handlebars_compiler_flag_strict; PG_RETURN_NULL(); }
EXTENSION(pg_handlebars_compiler_flag_string_params) { compiler_flags |= handlebars_compiler_flag_string_params; PG_RETURN_NULL(); }
EXTENSION(pg_handlebars_compiler_flag_track_ids) { compiler_flags |= handlebars_compiler_flag_track_ids; PG_RETURN_NULL(); }
EXTENSION(pg_handlebars_compiler_flag_use_data) { compiler_flags |= handlebars_compiler_flag_use_data; PG_RETURN_NULL(); }
EXTENSION(pg_handlebars_compiler_flag_use_depths) { compiler_flags |= handlebars_compiler_flag_use_depths; PG_RETURN_NULL(); }

EXTENSION(pg_handlebars_convert_input) { if (PG_ARGISNULL(0)) E("convert is null!"); convert_input = DatumGetBool(PG_GETARG_DATUM(0)); PG_RETURN_NULL(); }
EXTENSION(pg_handlebars_enable_partial_loader) { if (PG_ARGISNULL(0)) E("partial is null!"); enable_partial_loader = DatumGetBool(PG_GETARG_DATUM(0)); PG_RETURN_NULL(); }
EXTENSION(pg_handlebars_run_count) { if (PG_ARGISNULL(0)) E("run is null!"); run_count = DatumGetInt64(PG_GETARG_DATUM(0)); PG_RETURN_NULL(); }

static void pg_handlebars_clean(void) {
    handlebars_context_dtor(ctx);
    ctx = NULL;
    talloc_free(root);
    root = NULL;
    partial_extension = NULL;
    partial_path = NULL;
}

EXTENSION(pg_handlebars_partial_extension) {
    jmp_buf jmp;
    text *extension;
    if (PG_ARGISNULL(0)) E("extension is null!");
    extension = DatumGetTextP(PG_GETARG_DATUM(0));
    if (!root) root = talloc_new(NULL);
    if (!ctx) ctx = handlebars_context_ctor_ex(root);
    if (handlebars_setjmp_ex(ctx, &jmp)) {
        const char *error = pstrdup(handlebars_error_message(ctx));
        pg_handlebars_clean();
        E(error);
    }
    partial_extension = handlebars_string_ctor(ctx, VARDATA_ANY(extension), VARSIZE_ANY_EXHDR(extension));
    PG_RETURN_NULL();
}

EXTENSION(pg_handlebars_partial_path) {
    jmp_buf jmp;
    text *path;
    if (PG_ARGISNULL(0)) E("path is null!");
    path = DatumGetTextP(PG_GETARG_DATUM(0));
    if (!root) root = talloc_new(NULL);
    if (!ctx) ctx = handlebars_context_ctor_ex(root);
    if (handlebars_setjmp_ex(ctx, &jmp)) {
        const char *error = pstrdup(handlebars_error_message(ctx));
        pg_handlebars_clean();
        E(error);
    }
    partial_path = handlebars_string_ctor(ctx, VARDATA_ANY(path), VARSIZE_ANY_EXHDR(path));
    PG_RETURN_NULL();
}

EXTENSION(pg_handlebars) {
    jmp_buf jmp;
    struct handlebars_ast_node *ast;
    struct handlebars_compiler *compiler;
    struct handlebars_module *module;
    struct handlebars_parser *parser;
    struct handlebars_program *program;
    struct handlebars_string *buffer = NULL;
    struct handlebars_string *tmpl;
    struct handlebars_value *input;
    struct handlebars_value *partials;
    text *json;
    text *template;
    if (PG_ARGISNULL(0)) E("json is null!");
    if (PG_ARGISNULL(1)) E("template is null!");
    json = DatumGetTextP(PG_GETARG_DATUM(0));
    template = DatumGetTextP(PG_GETARG_DATUM(1));
    if (!root) root = talloc_new(NULL);
    if (!ctx) ctx = handlebars_context_ctor_ex(root);
    if (handlebars_setjmp_ex(ctx, &jmp)) {
        const char *error = pstrdup(handlebars_error_message(ctx));
        pg_handlebars_clean();
        E(error);
    }
    compiler = handlebars_compiler_ctor(ctx);
    handlebars_compiler_set_flags(compiler, compiler_flags);
    parser = handlebars_parser_ctor(ctx);
    tmpl = handlebars_string_ctor(HBSCTX(parser), VARDATA_ANY(template), VARSIZE_ANY_EXHDR(template));
    if (compiler_flags & handlebars_compiler_flag_compat) tmpl = handlebars_preprocess_delimiters(ctx, tmpl, NULL, NULL);
    ast = handlebars_parse_ex(parser, tmpl, compiler_flags);
    program = handlebars_compiler_compile_ex(compiler, ast);
    module = handlebars_program_serialize(ctx, program);
    input = handlebars_value_ctor(ctx);
    handlebars_value_init_json_string_length(ctx, input, VARDATA_ANY(json), VARSIZE_ANY_EXHDR(json));
    if (convert_input) handlebars_value_convert(input);
    if (enable_partial_loader) partials = handlebars_value_partial_loader_init(ctx, partial_path ? partial_path : handlebars_string_ctor(ctx, ".", sizeof(".") - 1), partial_extension ? partial_extension : handlebars_string_ctor(ctx, ".hbs", sizeof(".hbs") - 1), handlebars_value_ctor(ctx));
    do {
        struct handlebars_vm *vm = handlebars_vm_ctor(ctx);
        handlebars_vm_set_flags(vm, compiler_flags);
        if (enable_partial_loader) handlebars_vm_set_partials(vm, partials);
        if (buffer) handlebars_talloc_free(buffer);
        buffer = handlebars_vm_execute(vm, module, input);
        buffer = talloc_steal(ctx, buffer);
        handlebars_vm_dtor(vm);
    } while(--run_count > 0);
    handlebars_value_dtor(input);
    if (enable_partial_loader) handlebars_value_dtor(partials);
    switch (PG_NARGS()) {
        case 2: if (!buffer) {
            pg_handlebars_clean();
            PG_RETURN_NULL();
        } else {
            text *output = cstring_to_text_with_len(hbs_str_val(buffer), hbs_str_len(buffer));
            pg_handlebars_clean();
            PG_RETURN_TEXT_P(output);
        } break;
        case 3: if (!buffer) {
            pg_handlebars_clean();
            PG_RETURN_BOOL(false);
        } else {
            char *name;
            FILE *file;
            if (PG_ARGISNULL(2)) handlebars_throw(ctx, HANDLEBARS_ERROR, "file is null!");
            name = TextDatumGetCString(PG_GETARG_DATUM(2));
            if (!(file = fopen(name, "wb"))) handlebars_throw(ctx, HANDLEBARS_ERROR, "!fopen");
            pfree(name);
            fwrite(hbs_str_val(buffer), sizeof(char), hbs_str_len(buffer), file);
            fclose(file);
            pg_handlebars_clean();
            PG_RETURN_BOOL(true);
        } break;
        default: handlebars_throw(ctx, HANDLEBARS_ERROR, "expect be 2 or 3 args");
    }
}
