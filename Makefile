EXTENSION = pg_handlebars
MODULE_big = $(EXTENSION)

PG_CONFIG = pg_config
OBJS = $(EXTENSION).o pg_handlebars_json.o
DATA = pg_handlebars--1.0.sql

LIBS += -lhandlebars -ljson-c
SHLIB_LINK := $(LIBS)

PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)

$(OBJS): Makefile