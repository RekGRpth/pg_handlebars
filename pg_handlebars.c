#include <postgres.h>

#include <catalog/pg_type.h>
#include <fmgr.h>
#include <handlebars/handlebars_ast.h>
#include <handlebars/handlebars_ast_printer.h>
#include <handlebars/handlebars_cache.h>
#include <handlebars/handlebars_closure.h>
#include <handlebars/handlebars_compiler.h>
#include <handlebars/handlebars_delimiters.h>
#include <handlebars/handlebars.h>
#include <handlebars/handlebars_helpers.h>
#include <handlebars/handlebars_json.h>
#include <handlebars/handlebars_map.h>
#include <handlebars/handlebars_memory.h>
#include <handlebars/handlebars_module_printer.h>
#include <handlebars/handlebars_opcode_printer.h>
#include <handlebars/handlebars_opcode_serializer.h>
#include <handlebars/handlebars_opcodes.h>
#include <handlebars/handlebars_parser.h>
#include <handlebars/handlebars_partial_loader.h>
#include <handlebars/handlebars_stack.h>
#include <handlebars/handlebars_string.h>
#include <handlebars/handlebars_token.h>
#include <handlebars/handlebars_value.h>
#include <handlebars/handlebars_vm.h>
#include <handlebars/handlebars_yaml.h>
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

static size_t pool_size = 2 * 1024 * 1024;
static TALLOC_CTX *root;

void _PG_init(void); void _PG_init(void) {
#ifdef HANDLEBARS_HAVE_VALGRIND
    if (RUNNING_ON_VALGRIND) pool_size = 0;
#endif
    root = talloc_new(NULL);
    if (pool_size > 0) {
        void *old_root = root;
        root = talloc_pool(NULL, pool_size);
        talloc_steal(root, old_root);
    }
}

void _PG_fini(void); void _PG_fini(void) {
    talloc_free(root);
}

EXTENSION(pg_handlebars_compile) {
    jmp_buf jmp;
    struct handlebars_ast_node *ast;
    struct handlebars_compiler *compiler;
    struct handlebars_context *ctx;
    struct handlebars_parser *parser;
    struct handlebars_program *program;
    struct handlebars_string *print;
    struct handlebars_string *tmpl;
    text *template;
    unsigned long compiler_flags = 0;
    if (PG_ARGISNULL(0)) E("template is null!");
    if (PG_ARGISNULL(1)) E("flags is null!");
    template = DatumGetTextP(PG_GETARG_DATUM(0));
    compiler_flags = DatumGetUInt64(PG_GETARG_DATUM(1));
    ctx = handlebars_context_ctor_ex(root);
    if (handlebars_setjmp_ex(ctx, &jmp)) E(handlebars_error_message(ctx));
    parser = handlebars_parser_ctor(ctx);
    compiler = handlebars_compiler_ctor(ctx);
    handlebars_compiler_set_flags(compiler, compiler_flags);
    tmpl = handlebars_string_ctor(HBSCTX(ctx), VARDATA_ANY(template), VARSIZE_ANY_EXHDR(template));
    if (compiler_flags & handlebars_compiler_flag_compat) tmpl = handlebars_preprocess_delimiters(ctx, tmpl, NULL, NULL);
    ast = handlebars_parse_ex(parser, tmpl, compiler_flags);
    program = handlebars_compiler_compile_ex(compiler, ast);
    print = handlebars_program_print(ctx, program, 0);
    switch (PG_NARGS()) {
        case 2: {
            text *output = cstring_to_text_with_len(hbs_str_val(print), hbs_str_len(print));
            handlebars_context_dtor(ctx);
            PG_RETURN_TEXT_P(output);
        } break;
        case 3: {
            char *name;
            FILE *file;
            if (PG_ARGISNULL(2)) E("file is null!");
            name = TextDatumGetCString(PG_GETARG_DATUM(2));
            if (!(file = fopen(name, "wb"))) E("!fopen");
            pfree(name);
            fwrite(hbs_str_val(print), sizeof(char), hbs_str_len(print), file);
            fclose(file);
            handlebars_context_dtor(ctx);
            PG_RETURN_BOOL(true);
        } break;
        default: E("expect be 2 or 3 args");
    }
}

EXTENSION(pg_handlebars_execute) {
    bool convert_input = true;
    jmp_buf jmp;
    long run_count = 1;
    struct handlebars_ast_node *ast;
    struct handlebars_compiler *compiler;
    struct handlebars_context *ctx;
    struct handlebars_module *module;
    struct handlebars_parser *parser;
    struct handlebars_program *program;
    struct handlebars_string *buffer = NULL;
    struct handlebars_string *tmpl;
    struct handlebars_value *input;
    text *json;
    text *template;
    unsigned long compiler_flags = 0;
    if (PG_ARGISNULL(0)) E("json is null!");
    if (PG_ARGISNULL(1)) E("template is null!");
    if (PG_ARGISNULL(2)) E("flags is null!");
    if (PG_ARGISNULL(3)) E("convert is null!");
    if (PG_ARGISNULL(4)) E("run is null!");
    json = DatumGetTextP(PG_GETARG_DATUM(0));
    template = DatumGetTextP(PG_GETARG_DATUM(1));
    compiler_flags = DatumGetUInt64(PG_GETARG_DATUM(2));
    convert_input = DatumGetBool(PG_GETARG_DATUM(3));
    run_count = DatumGetInt64(PG_GETARG_DATUM(4));
    ctx = handlebars_context_ctor_ex(root);
    if (handlebars_setjmp_ex(ctx, &jmp)) E(handlebars_error_message(ctx));
    parser = handlebars_parser_ctor(ctx);
    compiler = handlebars_compiler_ctor(ctx);
    handlebars_compiler_set_flags(compiler, compiler_flags);
    tmpl = handlebars_string_ctor(HBSCTX(parser), VARDATA_ANY(template), VARSIZE_ANY_EXHDR(template));
    if (compiler_flags & handlebars_compiler_flag_compat) tmpl = handlebars_preprocess_delimiters(ctx, tmpl, NULL, NULL);
    handlebars_value_init_json_string_length(ctx, input, VARDATA_ANY(json), VARSIZE_ANY_EXHDR(json));
    if (convert_input) handlebars_value_convert(input);
    ast = handlebars_parse_ex(parser, tmpl, compiler_flags);
    program = handlebars_compiler_compile_ex(compiler, ast);
    module = handlebars_program_serialize(ctx, program);
    do {
        struct handlebars_vm *vm;
        if (buffer) {
            handlebars_talloc_free(buffer);
            buffer = NULL;
        }
        vm = handlebars_vm_ctor(ctx);
        handlebars_vm_set_flags(vm, compiler_flags);
        buffer = handlebars_vm_execute(vm, module, input);
        buffer = talloc_steal(ctx, buffer);
        handlebars_vm_dtor(vm);
    } while(--run_count > 0);
    switch (PG_NARGS()) {
        case 5: if (!buffer) PG_RETURN_NULL(); else {
            text *output = cstring_to_text_with_len(hbs_str_val(buffer), hbs_str_len(buffer));
            handlebars_context_dtor(ctx);
            PG_RETURN_TEXT_P(output);
        } break;
        case 6: if (!buffer) PG_RETURN_BOOL(false); else {
            char *name;
            FILE *file;
            if (PG_ARGISNULL(2)) E("file is null!");
            name = TextDatumGetCString(PG_GETARG_DATUM(2));
            if (!(file = fopen(name, "wb"))) E("!fopen");
            pfree(name);
            fwrite(hbs_str_val(buffer), sizeof(char), hbs_str_len(buffer), file);
            fclose(file);
            handlebars_context_dtor(ctx);
            PG_RETURN_BOOL(true);
        } break;
        default: E("expect be 5 or 6 args");
    }
}

EXTENSION(pg_handlebars_lex) {
    jmp_buf jmp;
    struct handlebars_context *ctx;
    struct handlebars_parser *parser;
    struct handlebars_string *tmpl;
    text *template;
    unsigned long compiler_flags = 0;
    if (PG_ARGISNULL(0)) E("template is null!");
    if (PG_ARGISNULL(1)) E("flags is null!");
    template = DatumGetTextP(PG_GETARG_DATUM(0));
    compiler_flags = DatumGetUInt64(PG_GETARG_DATUM(1));
    ctx = handlebars_context_ctor_ex(root);
    if (handlebars_setjmp_ex(ctx, &jmp)) E(handlebars_error_message(ctx));
    tmpl = handlebars_string_ctor(HBSCTX(ctx), VARDATA_ANY(template), VARSIZE_ANY_EXHDR(template));
    if (compiler_flags & handlebars_compiler_flag_compat) tmpl = handlebars_preprocess_delimiters(ctx, tmpl, NULL, NULL);
    parser = handlebars_parser_ctor(ctx);
    switch (PG_NARGS()) {
        case 2: {
            text *output = cstring_to_text_with_len("", 0);
            for (struct handlebars_token **tokens = handlebars_lex_ex(parser, tmpl); *tokens; tokens++ ) {
                struct handlebars_string *tmp = handlebars_token_print(ctx, *tokens, 1);
                text *out = cstring_to_text_with_len(hbs_str_val(tmp), hbs_str_len(tmp));
                handlebars_talloc_free(tmp);
                output = DatumGetTextP(DirectFunctionCall2(text_concat, PointerGetDatum(output), PointerGetDatum(out)));
                pfree(out);
            }
            handlebars_context_dtor(ctx);
            PG_RETURN_TEXT_P(output);
        } break;
        case 3: {
            char *name;
            FILE *file;
            if (PG_ARGISNULL(2)) E("file is null!");
            name = TextDatumGetCString(PG_GETARG_DATUM(2));
            if (!(file = fopen(name, "wb"))) E("!fopen");
            pfree(name);
            for (struct handlebars_token **tokens = handlebars_lex_ex(parser, tmpl); *tokens; tokens++ ) {
                struct handlebars_string *tmp = handlebars_token_print(ctx, *tokens, 1);
                fwrite(hbs_str_val(tmp), sizeof(char), hbs_str_len(tmp), file);
                handlebars_talloc_free(tmp);
            }
            fclose(file);
            handlebars_context_dtor(ctx);
            PG_RETURN_BOOL(true);
        } break;
        default: E("expect be 2 or 3 args");
    }
}

EXTENSION(pg_handlebars_module) {
    bool pretty_print = true;
    jmp_buf jmp;
    struct handlebars_ast_node *ast;
    struct handlebars_compiler *compiler;
    struct handlebars_context *ctx;
    struct handlebars_module *module;
    struct handlebars_parser *parser;
    struct handlebars_program *program;
    struct handlebars_string *print;
    struct handlebars_string *tmpl;
    text *template;
    unsigned long compiler_flags = 0;
    if (PG_ARGISNULL(0)) E("template is null!");
    if (PG_ARGISNULL(1)) E("flags is null!");
    if (PG_ARGISNULL(2)) E("pretty is null!");
    template = DatumGetTextP(PG_GETARG_DATUM(0));
    compiler_flags = DatumGetUInt64(PG_GETARG_DATUM(1));
    pretty_print = DatumGetBool(PG_GETARG_DATUM(2));
    ctx = handlebars_context_ctor_ex(root);
    if (handlebars_setjmp_ex(ctx, &jmp)) E(handlebars_error_message(ctx));
    parser = handlebars_parser_ctor(ctx);
    compiler = handlebars_compiler_ctor(ctx);
    handlebars_compiler_set_flags(compiler, compiler_flags);
    tmpl = handlebars_string_ctor(HBSCTX(ctx), VARDATA_ANY(template), VARSIZE_ANY_EXHDR(template));
    if (compiler_flags & handlebars_compiler_flag_compat) tmpl = handlebars_preprocess_delimiters(ctx, tmpl, NULL, NULL);
    ast = handlebars_parse_ex(parser, tmpl, compiler_flags);
    program = handlebars_compiler_compile_ex(compiler, ast);
    module = handlebars_program_serialize(ctx, program);
    handlebars_module_generate_hash(module);
    switch (PG_NARGS()) {
        case 3: {
            text *output;
            if (pretty_print) {
                print = handlebars_module_print(ctx, module);
                output = cstring_to_text_with_len(hbs_str_val(print), hbs_str_len(print));
            } else {
                handlebars_module_normalize_pointers(module, (void *) 0);
                output = cstring_to_text_with_len((char *)module, handlebars_module_get_size(module));
            }
            handlebars_context_dtor(ctx);
            PG_RETURN_TEXT_P(output);
        } break;
        case 4: {
            char *name;
            FILE *file;
            if (PG_ARGISNULL(3)) E("file is null!");
            name = TextDatumGetCString(PG_GETARG_DATUM(3));
            if (!(file = fopen(name, "wb"))) E("!fopen");
            pfree(name);
            if (pretty_print) {
                print = handlebars_module_print(ctx, module);
                fwrite(hbs_str_val(print), sizeof(char), hbs_str_len(print), file);
            } else {
                handlebars_module_normalize_pointers(module, (void *) 0);
                fwrite((char *) module, sizeof(char), handlebars_module_get_size(module), file);
            }
            fclose(file);
            handlebars_context_dtor(ctx);
            PG_RETURN_BOOL(true);
        } break;
        default: E("expect be 3 or 4 args");
    }
}

EXTENSION(pg_handlebars_parse) {
    jmp_buf jmp;
    struct handlebars_ast_node *ast;
    struct handlebars_context *ctx;
    struct handlebars_parser *parser;
    struct handlebars_string *print;
    struct handlebars_string *tmpl;
    text *template;
    unsigned long compiler_flags = 0;
    if (PG_ARGISNULL(0)) E("template is null!");
    if (PG_ARGISNULL(1)) E("flags is null!");
    template = DatumGetTextP(PG_GETARG_DATUM(0));
    compiler_flags = DatumGetUInt64(PG_GETARG_DATUM(1));
    ctx = handlebars_context_ctor_ex(root);
    if (handlebars_setjmp_ex(ctx, &jmp)) E(handlebars_error_message(ctx));
    tmpl = handlebars_string_ctor(HBSCTX(ctx), VARDATA_ANY(template), VARSIZE_ANY_EXHDR(template));
    if (compiler_flags & handlebars_compiler_flag_compat) tmpl = handlebars_preprocess_delimiters(ctx, tmpl, NULL, NULL);
    parser = handlebars_parser_ctor(ctx);
    ast = handlebars_parse_ex(parser, tmpl, compiler_flags);
    print = handlebars_ast_print(HBSCTX(parser), ast);
    switch (PG_NARGS()) {
        case 2: {
            text *output = cstring_to_text_with_len(hbs_str_val(print), hbs_str_len(print));
            handlebars_context_dtor(ctx);
            PG_RETURN_TEXT_P(output);
        } break;
        case 3: {
            char *name;
            FILE *file;
            if (PG_ARGISNULL(2)) E("file is null!");
            name = TextDatumGetCString(PG_GETARG_DATUM(2));
            if (!(file = fopen(name, "wb"))) E("!fopen");
            pfree(name);
            fwrite(hbs_str_val(print), sizeof(char), hbs_str_len(print), file);
            fclose(file);
            handlebars_context_dtor(ctx);
            PG_RETURN_BOOL(true);
        } break;
        default: E("expect be 2 or 3 args");
    }
}
