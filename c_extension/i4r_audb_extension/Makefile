#
PGFILEDESC = "C I4R AUDB Extension"
EXTENSION = i4r_audb_extension
MODULE_big = i4r_audb_extension
DATA = i4r_audb_extension--1.1.sql
OBJS = 	\
	arithmetic.o \
	helperFunctions.o \
	serialization.o	\
	logicalOperators.o	\
	prune.o \
	i4r_audb_extension.o

# using postgres@16 for local testing
# PG_CONFIG = /usr/local/pgsql16/bin/pg_config
PG_CONFIG ?= pg_config
PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)