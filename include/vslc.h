#ifndef VSLC_H
#define VSLC_H
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdarg.h>

#include "tlhash.h"
#include "nodetypes.h"
#include "ir.h"
#include "y.tab.h"

int yyerror ( const char *error );
extern int yylineno;
extern int yylex ( void );
extern char yytext[];

extern node_t *root;

/* Global state */
extern node_t *root;

// Moving global defs to global header

extern tlhash_t *global_names;  // Defined in ir.c, used by generator.c
extern char **string_list;      // Defined in ir.c, used by generator.c
extern size_t stringc;          // Defined in ir.c, used by generator.c

/* Global routines, called from main in vslc.c */
void simplify_tree (node_t **simplified, node_t *root);
void node_print(node_t *root, int nesting);
void destroy_subtree ( node_t *discard );

void create_symbol_table ( void );
void print_symbol_table ( void );
void destroy_symbol_table ( void );

void generate_program(void);

#endif
