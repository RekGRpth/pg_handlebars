#include <postgres.h>

#include <handlebars/handlebars_compiler.h>
#include <handlebars/handlebars_delimiters.h>
#include <handlebars/handlebars_json.h>
#include <handlebars/handlebars_memory.h>
#include <handlebars/handlebars_opcode_serializer.h>
#include <handlebars/handlebars_parser.h>
#include <handlebars/handlebars_partial_loader.h>
#include <handlebars/handlebars_string.h>
#include <handlebars/handlebars_value.h>
#include <handlebars/handlebars_vm.h>
#include <utils/builtins.h>

#define EXTENSION(function) Datum (function)(PG_FUNCTION_ARGS); PG_FUNCTION_INFO_V1(function); Datum (function)(PG_FUNCTION_ARGS)

#define D1(...) ereport(DEBUG1, (errmsg(__VA_ARGS__)))
#define D2(...) ereport(DEBUG2, (errmsg(__VA_ARGS__)))
#define D3(...) ereport(DEBUG3, (errmsg(__VA_ARGS__)))
#define D4(...) ereport(DEBUG4, (errmsg(__VA_ARGS__)))
#define D5(...) ereport(DEBUG5, (errmsg(__VA_ARGS__)))
#define E(...) ereport(ERROR, (errmsg(__VA_ARGS__)))
#define F(...) ereport(FATAL, (errmsg(__VA_ARGS__)))
#define I(...) ereport(INFO, (errmsg(__VA_ARGS__)))
#define L(...) ereport(LOG, (errmsg(__VA_ARGS__)))
#define N(...) ereport(NOTICE, (errmsg(__VA_ARGS__)))
#define W(...) ereport(WARNING, (errmsg(__VA_ARGS__)))

PG_MODULE_MAGIC;

static unsigned long compiler_flags = handlebars_compiler_flag_alternate_decorators|handlebars_compiler_flag_compat|handlebars_compiler_flag_explicit_partial_context|handlebars_compiler_flag_ignore_standalone|handlebars_compiler_flag_known_helpers_only|handlebars_compiler_flag_mustache_style_lambdas|handlebars_compiler_flag_prevent_indent|handlebars_compiler_flag_use_data|handlebars_compiler_flag_use_depths;

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

EXTENSION(pg_handlebars) {
    jmp_buf jmp;
    struct handlebars_ast_node *ast;
    struct handlebars_compiler *compiler;
    struct handlebars_context *ctx;
    struct handlebars_module *module;
    struct handlebars_parser *parser;
    struct handlebars_program *program;
    struct handlebars_string *buffer;
    struct handlebars_string *tmpl;
    struct handlebars_value *input;
    struct handlebars_value *partials;
    struct handlebars_vm *vm;
    text *json;
    text *template;
    if (PG_ARGISNULL(0)) E("json is null!");
    if (PG_ARGISNULL(1)) E("template is null!");
    json = DatumGetTextP(PG_GETARG_DATUM(0));
    template = DatumGetTextP(PG_GETARG_DATUM(1));
    ctx = handlebars_context_ctor();
    if (handlebars_setjmp_ex(ctx, &jmp)) {
        const char *error = pstrdup(handlebars_error_message(ctx));
        handlebars_context_dtor(ctx);
        E("%s", error);
    }
    compiler = handlebars_compiler_ctor(ctx);
    handlebars_compiler_set_flags(compiler, compiler_flags);
    parser = handlebars_parser_ctor(ctx);
    tmpl = handlebars_string_ctor(ctx, VARDATA_ANY(template), VARSIZE_ANY_EXHDR(template));
    if (compiler_flags & handlebars_compiler_flag_compat) tmpl = handlebars_preprocess_delimiters(ctx, tmpl, NULL, NULL);
    ast = handlebars_parse_ex(parser, tmpl, compiler_flags);
    program = handlebars_compiler_compile_ex(compiler, ast);
    module = handlebars_program_serialize(ctx, program);
    input = handlebars_value_ctor(ctx);
    buffer = handlebars_string_ctor(ctx, VARDATA_ANY(json), VARSIZE_ANY_EXHDR(json));
    handlebars_value_init_json_string(ctx, input, hbs_str_val(buffer));
    handlebars_value_convert(input);
    partials = handlebars_value_partial_loader_init(ctx, handlebars_string_ctor(ctx, ".", sizeof(".") - 1), handlebars_string_ctor(ctx, ".hbs", sizeof(".hbs") - 1), handlebars_value_ctor(ctx));
    vm = handlebars_vm_ctor(ctx);
    handlebars_vm_set_flags(vm, compiler_flags);
    handlebars_vm_set_partials(vm, partials);
    buffer = talloc_steal(ctx, handlebars_vm_execute(vm, module, input));
    handlebars_vm_dtor(vm);
    handlebars_value_dtor(input);
    handlebars_value_dtor(partials);
    switch (PG_NARGS()) {
        case 2: if (!buffer) {
            handlebars_context_dtor(ctx);
            PG_RETURN_NULL();
        } else {
            text *output = cstring_to_text_with_len(hbs_str_val(buffer), hbs_str_len(buffer));
            handlebars_context_dtor(ctx);
            PG_RETURN_TEXT_P(output);
        } break;
        case 3: if (!buffer) {
            handlebars_context_dtor(ctx);
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
            handlebars_context_dtor(ctx);
            PG_RETURN_BOOL(true);
        } break;
        default: handlebars_throw(ctx, HANDLEBARS_ERROR, "expect be 2 or 3 args");
    }
}
