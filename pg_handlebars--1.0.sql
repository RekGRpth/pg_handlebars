-- complain if script is sourced in psql, rather than via CREATE EXTENSION
\echo Use "CREATE EXTENSION pg_handlebars" to load this file. \quit

CREATE FUNCTION handlebars_compiler_flag_all() RETURNS void AS 'MODULE_PATHNAME', 'pg_handlebars_compiler_flag_all' LANGUAGE 'c';
CREATE FUNCTION handlebars_compiler_flag_alternate_decorators() RETURNS void AS 'MODULE_PATHNAME', 'pg_handlebars_compiler_flag_alternate_decorators' LANGUAGE 'c';
CREATE FUNCTION handlebars_compiler_flag_assume_objects() RETURNS void AS 'MODULE_PATHNAME', 'pg_handlebars_compiler_flag_assume_objects' LANGUAGE 'c';
CREATE FUNCTION handlebars_compiler_flag_compat() RETURNS void AS 'MODULE_PATHNAME', 'pg_handlebars_compiler_flag_compat' LANGUAGE 'c';
CREATE FUNCTION handlebars_compiler_flag_explicit_partial_context() RETURNS void AS 'MODULE_PATHNAME', 'pg_handlebars_compiler_flag_explicit_partial_context' LANGUAGE 'c';
CREATE FUNCTION handlebars_compiler_flag_ignore_standalone() RETURNS void AS 'MODULE_PATHNAME', 'pg_handlebars_compiler_flag_ignore_standalone' LANGUAGE 'c';
CREATE FUNCTION handlebars_compiler_flag_known_helpers_only() RETURNS void AS 'MODULE_PATHNAME', 'pg_handlebars_compiler_flag_known_helpers_only' LANGUAGE 'c';
CREATE FUNCTION handlebars_compiler_flag_mustache_style_lambdas() RETURNS void AS 'MODULE_PATHNAME', 'pg_handlebars_compiler_flag_mustache_style_lambdas' LANGUAGE 'c';
CREATE FUNCTION handlebars_compiler_flag_no_escape() RETURNS void AS 'MODULE_PATHNAME', 'pg_handlebars_compiler_flag_no_escape' LANGUAGE 'c';
CREATE FUNCTION handlebars_compiler_flag_none() RETURNS void AS 'MODULE_PATHNAME', 'pg_handlebars_compiler_flag_none' LANGUAGE 'c';
CREATE FUNCTION handlebars_compiler_flag_prevent_indent() RETURNS void AS 'MODULE_PATHNAME', 'pg_handlebars_compiler_flag_prevent_indent' LANGUAGE 'c';
CREATE FUNCTION handlebars_compiler_flag_strict() RETURNS void AS 'MODULE_PATHNAME', 'pg_handlebars_compiler_flag_strict' LANGUAGE 'c';
CREATE FUNCTION handlebars_compiler_flag_string_params() RETURNS void AS 'MODULE_PATHNAME', 'pg_handlebars_compiler_flag_string_params' LANGUAGE 'c';
CREATE FUNCTION handlebars_compiler_flag_track_ids() RETURNS void AS 'MODULE_PATHNAME', 'pg_handlebars_compiler_flag_track_ids' LANGUAGE 'c';
CREATE FUNCTION handlebars_compiler_flag_use_data() RETURNS void AS 'MODULE_PATHNAME', 'pg_handlebars_compiler_flag_use_data' LANGUAGE 'c';
CREATE FUNCTION handlebars_compiler_flag_use_depths() RETURNS void AS 'MODULE_PATHNAME', 'pg_handlebars_compiler_flag_use_depths' LANGUAGE 'c';

CREATE FUNCTION handlebars(json JSON, template TEXT) RETURNS TEXT AS 'MODULE_PATHNAME', 'pg_handlebars' LANGUAGE 'c';
CREATE FUNCTION handlebars(json JSON, template TEXT, file TEXT) RETURNS BOOL AS 'MODULE_PATHNAME', 'pg_handlebars' LANGUAGE 'c';
