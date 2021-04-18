#include "vslc.h"

#define MIN(a, b) (((a) < (b)) ? (a) : (b))
static const char *record[6] = {
    "%rdi", "%rsi", "%rdx", "%rcx", "%r8", "%r9"};

static void
generate_stringtable(void)
{
    /* These can be used to emit numbers, strings and a run-time
     * error msg. from main
     */
    puts(".data");
    puts("intout: .asciz \"\%ld \"");
    puts("strout: .asciz \"\%s \"");
    puts("errout: .asciz \"Wrong number of arguments\"");

    /* TODO:  handle the strings from the program */
    for (int i = 0; i < stringc; i++)
    {
        printf("STR%d:\t.asciz \"%s\": \n", i, string_list[i]);
    }
}

static void generate_global_vars(void)
{
    puts(".data");
    size_t gname_size = tlhash_size(global_names);
    symbol_t **gnames = malloc(gname_size * sizeof(symbol_t *));
    tlhash_values(global_names, gnames);
    for (int i = 0; i < tlhash_size(global_names); i++)
    {
        symbol_t *curr_sym = gnames[i];
        if (curr_sym->type == SYM_GLOBAL_VAR)
        {
            printf("%s: .zero 8\n", curr_sym->name);
        }
    }
}

static void
generate_main(symbol_t *first)
{
    puts(".globl main");
    puts(".text");
    puts("main:");
    puts("\tpushq %rbp");
    puts("\tmovq %rsp, %rbp");

    puts("\tsubq $1, %rdi");
    printf("\tcmpq\t$%zu,%%rdi\n", first->nparms);
    puts("\tjne ABORT");
    puts("\tcmpq $0, %rdi");
    puts("\tjz SKIP_ARGS");

    puts("\tmovq %rdi, %rcx");
    printf("\taddq $%zu, %%rsi\n", 8 * first->nparms);
    puts("PARSE_ARGV:");
    puts("\tpushq %rcx");
    puts("\tpushq %rsi");

    puts("\tmovq (%rsi), %rdi");
    puts("\tmovq $0, %rsi");
    puts("\tmovq $10, %rdx");
    puts("\tcall strtol");

    /*  Now a new argument is an integer in rax */
    puts("\tpopq %rsi");
    puts("\tpopq %rcx");
    puts("\tpushq %rax");
    puts("\tsubq $8, %rsi");
    puts("\tloop PARSE_ARGV");

    /* Now the arguments are in order on stack */
    for (int arg = 0; arg < MIN(6, first->nparms); arg++)
        printf("\tpopq\t%s\n", record[arg]);

    puts("SKIP_ARGS:");
    printf("\tcall\t_%s\n", first->name);
    puts("\tjmp END");
    puts("ABORT:");
    puts("\tmovq $errout, %rdi");
    puts("\tcall puts");

    puts("END:");
    puts("\tmovq %rax, %rdi");
    puts("\tcall exit");
}

void generate_program(void)
{
    generate_stringtable();
    generate_global_vars();

    /* Put some dummy stuff to keep the skeleton from crashing */
    puts(".globl main");
    puts(".text");
    puts("main:");
    puts("\tmovq $0, %rax");
    puts("\tcall exit");
}
