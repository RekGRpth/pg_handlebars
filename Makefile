$(OBJS): Makefile
DATA = pg_handlebars--1.0.sql
EXTENSION = pg_handlebars
MODULE_big = $(EXTENSION)
OBJS = $(EXTENSION).o
PG_CONFIG = pg_config
PGXS = $(shell $(PG_CONFIG) --pgxs)
SHLIB_LINK = -lhandlebars
include $(PGXS)
