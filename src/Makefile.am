bin_PROGRAMS = scrap500

noinst_HEADERS = scrap500.h

AM_CFLAGS = $(SQLITE3_CFLAGS) $(LIBXML_CFLAGS) $(CURL_CFLAGS)

AM_LDFLAGS = $(SQLITE3_LIBS) $(LIBXML_LIBS) $(CURL_LIBS) -pthread

scrap500_SOURCES = scrap500.c \
                   scrap500-http.c \
                   scrap500-parser.c \
                   scrap500-db.c

CLEANFILES = $(bin_PROGRAMS)

