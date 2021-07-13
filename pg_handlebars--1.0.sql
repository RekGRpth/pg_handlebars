-- complain if script is sourced in psql, rather than via CREATE EXTENSION
\echo Use "CREATE EXTENSION pg_handlebars" to load this file. \quit

CREATE OR REPLACE FUNCTION handlebars(json JSON, template TEXT, flags int8 default 0, convert bool default true, run int8 default 1, partial bool default true, path text default '.', extension text default '.hbs') RETURNS TEXT AS 'MODULE_PATHNAME', 'pg_handlebars' LANGUAGE 'c';
CREATE OR REPLACE FUNCTION handlebars(json JSON, template TEXT, file TEXT, flags int8 default 0, convert bool default true, run int8 default 1, partial bool default false, path text default '.', extension text default '.hbs') RETURNS BOOL AS 'MODULE_PATHNAME', 'pg_handlebars' LANGUAGE 'c';
