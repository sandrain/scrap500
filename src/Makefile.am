bin_PROGRAMS = scrap500 \
               scrap500-fetch \
               scrap500-build \
               getsysattrs

noinst_HEADERS = scrap500.h

AM_CFLAGS = $(SQLITE3_CFLAGS) $(LIBXML_CFLAGS) $(CURL_CFLAGS)

AM_LDFLAGS = $(SQLITE3_LIBS) $(LIBXML_LIBS) $(CURL_LIBS) -pthread

scrap500_SOURCES = scrap500.c \
                   scrap500-http.c \
                   scrap500-parser.c \
                   scrap500-db.c

scrap500_fetch_SOURCES = scrap500-fetch.c \
                         scrap500-http.c \
                         scrap500-parser.c

scrap500_build_SOURCES = scrap500-build.c \
                         scrap500-parser.c \
			 scrap500-schema.c

getsysattrs_SOURCES = getsysattrs.c

scrap500-schema.c: scrap500.schema.sqlite3.sql
	@( echo "const char schema_sqlstr[] = ";\
	   sed 's/^/"/; s/$$/\\n"/' < $< ;\
	   echo ";" ) > $@

CLEANFILES = $(bin_PROGRAMS) scrap500-schema.c

