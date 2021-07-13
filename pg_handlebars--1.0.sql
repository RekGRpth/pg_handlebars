-- complain if script is sourced in psql, rather than via CREATE EXTENSION
\echo Use "CREATE EXTENSION pg_handlebars" to load this file. \quit

CREATE OR REPLACE FUNCTION handlebars_compile(template TEXT, flags int8 default 0) RETURNS TEXT AS 'MODULE_PATHNAME', 'pg_handlebars_compile' LANGUAGE 'c';
CREATE OR REPLACE FUNCTION handlebars_compile(template TEXT, flags int8 default 0, file TEXT) RETURNS BOOL AS 'MODULE_PATHNAME', 'pg_handlebars_compile' LANGUAGE 'c';
