LEX=flex
YACC=bison
YFLAGS+=--defines=src/y.tab.h -o y.tab.c
CFLAGS+=-std=c99 -g -Isrc -Iinclude -D_POSIX_C_SOURCE=200809L -DYYSTYPE="node_t *"
LDLIBS+=-lc

src/vslc: src/vslc.c src/parser.o src/scanner.o src/nodetypes.o src/tree.o src/ir.o src/tlhash.c src/generator.o
src/y.tab.h: src/parser.c
src/scanner.c: src/y.tab.h src/scanner.l
clean:
	-rm -f src/parser.c src/scanner.c src/*.tab.* src/*.o
purge: clean
	-rm -f src/vslc

full: purge src/vslc
	-cd ./vsl_programs && make purge && make

full_purge: purge
	-cd ./vsl_programs && make purge

.PHONY:run
run: src/vslc
	./src/vslc < ./vsl_programs/_test.vsl > make_run_output.s
