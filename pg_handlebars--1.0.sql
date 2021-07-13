-- complain if script is sourced in psql, rather than via CREATE EXTENSION
\echo Use "CREATE EXTENSION pg_handlebars" to load this file. \quit

CREATE OR REPLACE FUNCTION handlebars_compile(template TEXT, flags int8 default 0) RETURNS TEXT AS 'MODULE_PATHNAME', 'pg_handlebars_compile' LANGUAGE 'c';
CREATE OR REPLACE FUNCTION handlebars_compile(template TEXT, flags int8 default 0, file TEXT default null) RETURNS BOOL AS 'MODULE_PATHNAME', 'pg_handlebars_compile' LANGUAGE 'c';

CREATE OR REPLACE FUNCTION handlebars_execute(json JSON, template TEXT, flags int8 default 0, convert bool default true, run int8 default 1) RETURNS TEXT AS 'MODULE_PATHNAME', 'pg_handlebars_execute' LANGUAGE 'c';
CREATE OR REPLACE FUNCTION handlebars_execute(json JSON, template TEXT, flags int8 default 0, convert bool default true, run int8 default 1, file TEXT default null) RETURNS BOOL AS 'MODULE_PATHNAME', 'pg_handlebars_execute' LANGUAGE 'c';

CREATE OR REPLACE FUNCTION handlebars_lex(template TEXT, flags int8 default 0) RETURNS TEXT AS 'MODULE_PATHNAME', 'pg_handlebars_lex' LANGUAGE 'c';
CREATE OR REPLACE FUNCTION handlebars_lex(template TEXT, flags int8 default 0, file TEXT default null) RETURNS BOOL AS 'MODULE_PATHNAME', 'pg_handlebars_lex' LANGUAGE 'c';

CREATE OR REPLACE FUNCTION handlebars_module(template TEXT, flags int8 default 0, pretty bool default true) RETURNS TEXT AS 'MODULE_PATHNAME', 'pg_handlebars_module' LANGUAGE 'c';
CREATE OR REPLACE FUNCTION handlebars_module(template TEXT, flags int8 default 0, pretty bool default true, file TEXT default null) RETURNS BOOL AS 'MODULE_PATHNAME', 'pg_handlebars_module' LANGUAGE 'c';

CREATE OR REPLACE FUNCTION handlebars_parse(template TEXT, flags int8 default 0) RETURNS TEXT AS 'MODULE_PATHNAME', 'pg_handlebars_parse' LANGUAGE 'c';
CREATE OR REPLACE FUNCTION handlebars_parse(template TEXT, flags int8 default 0, file TEXT default null) RETURNS BOOL AS 'MODULE_PATHNAME', 'pg_handlebars_parse' LANGUAGE 'c';
