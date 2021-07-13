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
//static struct handlebars_context *ctx;
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
//    ctx = handlebars_context_ctor_ex(root);
}

void _PG_fini(void); void _PG_fini(void) {
//    handlebars_context_dtor(ctx);
    talloc_free(root);
}

EXTENSION(pg_handlebars_compile) {
    FILE *file;
    jmp_buf jmp;
    struct handlebars_ast_node *ast;
    struct handlebars_compiler *compiler;
    struct handlebars_context *ctx;
    struct handlebars_parser *parser;
    struct handlebars_program *program;
    struct handlebars_string *print;
    struct handlebars_string *tmpl;
    text *output;
    text *template;
    unsigned long compiler_flags = 0;
    if (PG_ARGISNULL(0)) E("template is null!");
    if (PG_ARGISNULL(1)) E("flags is null!");
    template = DatumGetTextP(PG_GETARG_DATUM(0));
    compiler_flags = DatumGetUInt64(PG_GETARG_DATUM(1));
    switch (PG_NARGS()) {
        case 2: break;
        case 3: {
            char *name;
            if (PG_ARGISNULL(2)) E("file is null!");
            name = TextDatumGetCString(PG_GETARG_DATUM(2));
            if (!(file = fopen(name, "wb"))) E("!fopen");
            pfree(name);
        } break;
        default: E("expect be 2 or 3 args");
    }
    ctx = handlebars_context_ctor_ex(root);
    if (handlebars_setjmp_ex(ctx, &jmp)) E(handlebars_error_message(ctx));
    compiler = handlebars_compiler_ctor(ctx);
    handlebars_compiler_set_flags(compiler, compiler_flags);
    tmpl = handlebars_string_ctor(HBSCTX(ctx), VARDATA_ANY(template), VARSIZE_ANY_EXHDR(template));
    if (compiler_flags & handlebars_compiler_flag_compat) tmpl = handlebars_preprocess_delimiters(ctx, tmpl, NULL, NULL);
    parser = handlebars_parser_ctor(ctx);
    ast = handlebars_parse_ex(parser, tmpl, compiler_flags);
    program = handlebars_compiler_compile_ex(compiler, ast);
    print = handlebars_program_print(ctx, program, 0);
    switch (PG_NARGS()) {
        case 2:
            output = cstring_to_text_with_len(hbs_str_val(print), hbs_str_len(print));
            handlebars_context_dtor(ctx);
            PG_RETURN_TEXT_P(output);
            break;
        case 3:
            fwrite(hbs_str_val(print), sizeof(char), hbs_str_len(print), file);
            fclose(file);
            handlebars_context_dtor(ctx);
            PG_RETURN_BOOL(true);
            break;
        default: E("expect be 2 or 3 args");
    }
}

EXTENSION(pg_handlebars_execute) {}
EXTENSION(pg_handlebars_lex) {}
EXTENSION(pg_handlebars_module) {}
EXTENSION(pg_handlebars_parse) {}
