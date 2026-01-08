CC := gcc

.PHONY: all clean debug dev install

all: dist/cmpr

CFLAGS := -O2 -Wall
LDFLAGS := -lm

debug: CFLAGS := -g -O0 -Wall -fsanitize=address
debug: dist/cmpr

dev: CFLAGS := -g -O2 -Wall -Werror -fsanitize=address
dev: dist/cmpr

dist/cmpr: cmpr.c fdecls.h bootstrap_content.c help_topics.c siphash/siphash.o siphash/halfsiphash.o
	mkdir -p dist
	(VER=9; D=$$(date +%Y%m%d-%H%M%S); GIT=$$(git log -1 --pretty="%h %f"); echo '#line 1 "cmpr.c"' >cmpr-sed.c; sed 's/\$$VERSION\$$/'"$$VER"' (build: '"$$D"' '"$$GIT"')/' <cmpr.c >>cmpr-sed.c; cat bootstrap_content.c >>cmpr-sed.c; cat help_topics.c >>cmpr-sed.c; echo "Version: $$VER (build: $$D $$GIT)"; $(CC) -o dist/cmpr-$$D cmpr-sed.c siphash/siphash.o siphash/halfsiphash.o $(CFLAGS) $(LDFLAGS) && rm -f dist/cmpr && ln -s cmpr-$$D dist/cmpr)

siphash/siphash.o: siphash/siphash.c
	$(CC) -c siphash/siphash.c $(CFLAGS) -o siphash/siphash.o

siphash/halfsiphash.o: siphash/halfsiphash.c
	$(CC) -c siphash/halfsiphash.c $(CFLAGS) -o siphash/halfsiphash.o

clean:
	rm -f cmpr-sed.c
	rm -f siphash/*.o

install: dist/cmpr
	install -m 755 dist/cmpr /usr/local/bin/cmpr
