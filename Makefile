CC := gcc

.PHONY: all clean debug dev install

all: dist/cmpr

CFLAGS := -O2 -Wall
LDFLAGS := -lm

debug: CFLAGS := -g -O0 -Wall -fsanitize=address
debug: dist/cmpr

dev: CFLAGS := -g -O2 -Wall -Werror -fsanitize=address
dev: dist/cmpr

dist/cmpr: cmpr.c fdecls.h spanio.c bootstrap_content.c help_topics.c siphash/siphash.o siphash/halfsiphash.o
	mkdir -p dist
	(VER=9; D=$$(date +%Y%m%d-%H%M%S); GIT=$$(git log -1 --pretty="%h %f"); echo '#line 1 "cmpr.c"' >cmpr-sed.c; sed 's/\$$VERSION\$$/'"$$VER"' (build: '"$$D"' '"$$GIT"')/' <cmpr.c >>cmpr-sed.c; cat bootstrap_content.c >>cmpr-sed.c; cat help_topics.c >>cmpr-sed.c; echo "Version: $$VER (build: $$D $$GIT)"; $(CC) -o dist/cmpr-$$D cmpr-sed.c siphash/siphash.o siphash/halfsiphash.o $(CFLAGS) $(LDFLAGS) && rm -f dist/cmpr && ln -s cmpr-$$D dist/cmpr)

# Bootstrap note: This rule requires system 'cmpr' to be installed. For fresh builds,
# either keep the checked-in stub bootstrap_content.c, or run 'sudo make install' first.
bootstrap_content.c: INBOX.c
	cmpr --print-code '#generate_bootstrap' | bash > bootstrap_content.c

help_topics.c: INBOX.c
	cmpr --print-code '#generate_help_topics' | bash > help_topics.c

siphash/siphash.o: siphash/siphash.c
	$(CC) -c siphash/siphash.c $(CFLAGS) -o siphash/siphash.o

siphash/halfsiphash.o: siphash/halfsiphash.c
	$(CC) -c siphash/halfsiphash.c $(CFLAGS) -o siphash/halfsiphash.o

fdecls.h: cmpr-src.c bootstrap_content.c help_topics.c
	cat $^ | python3 extract_decls.py > fdecls.h

clean:
	rm -f cmpr-sed.c
	rm -f bootstrap_content.c help_topics.c
	rm -f siphash/*.o

install: dist/cmpr
	install -m 755 dist/cmpr /usr/local/bin/cmpr
