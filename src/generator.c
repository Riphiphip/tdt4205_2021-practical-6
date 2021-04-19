#include "vslc.h"

#define MIN(a, b) (((a) < (b)) ? (a) : (b))
static const char *record[6] = {
    "%rdi", "%rsi", "%rdx", "%rcx", "%r8", "%r9"};

// Used for generating unique label names for `if` and `while` statements
int while_id = 0;
int if_id = 0;

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
        printf("STR%d:\t.asciz %s: \n", i, string_list[i]);
    }
}

static void generate_global_vars(void)
{
    puts(".data");
    size_t gname_size = tlhash_size(global_names);
    symbol_t **gnames = malloc(gname_size * sizeof(symbol_t *));
    tlhash_values(global_names, (void **)gnames);
    for (int i = 0; i < tlhash_size(global_names); i++)
    {
        symbol_t *curr_sym = gnames[i];
        if (curr_sym->type == SYM_GLOBAL_VAR)
        {
            printf("%s:\t.zero 8\n", curr_sym->name);
        }
    }
    free(gnames);
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
    printf("\tcmpq $%zu,%%rdi\n", first->nparms);
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

static void generate_global_access(symbol_t *symbol)
{
    printf("\tmovq %s(%%rip), %%rax\n", symbol->name);
}

static void generate_parameter_access(symbol_t *symbol)
{
    printf("\tmovq (%%rbp, $%ld, $8), %%rax\n", symbol->seq);
}

static void generate_local_access(symbol_t *symbol, symbol_t *function)
{
    printf("\tmovq %%rax, $%#lx(%%rbp, $%ld, $8)\n", ALIGNED_VARIABLES(function->nparms), symbol->seq);
}

static void generate_access(symbol_t *symbol, symbol_t *function)
{
    // printf("/* Access to variable \"%s\"*/\n", symbol->name);
    switch (symbol->type)
    {
    case SYM_GLOBAL_VAR:
        generate_global_access(symbol);
        break;
    case SYM_PARAMETER:
        generate_parameter_access(symbol);
        break;
    case SYM_LOCAL_VAR:
        generate_local_access(symbol, function);
        break;
    }
}

static void generate_relation(node_t *node, symbol_t *function, scope s)
{
    
}

static void generate_expression(node_t *node, symbol_t *function, scope s)
{
    switch (node->type)
    {
    case IDENTIFIER_DATA:
    {
        if (node->entry != NULL)
        {
            if (node->entry->type == SYM_FUNCTION)
            {
                //TODO: Generate function calls
            }
            else
            {
                return generate_access(node->entry, function);
            }
        }
        break;
    }
    case NUMBER_DATA:
    {
        printf("\tmovq $%ld, %%rax\n", *(long *)node->data);
        return; 
    }
    }
}

static void generate_global_assignment(symbol_t *symbol)
{
    printf("movq %%rax, %s(%%rip)", symbol->name);
}

static void generate_parameter_assignment(symbol_t *symbol)
{
    printf("\tmovq %%rax, (%%rbp, $%ld, $8)\n", symbol->seq);
}

static void generate_local_assignment(symbol_t *symbol, symbol_t *function)
{
    printf("\tmovq %%rax, $%#lx(%%rbp, $%ld, $8)\n", ALIGNED_VARIABLES(function->nparms), symbol->seq);
}

static void generate_assignment(node_t *node, symbol_t *function, scope s)
{
    generate_expression(node->children[1], function, s);
    switch (node->children[0]->entry->type)
    {
    case SYM_GLOBAL_VAR:
        generate_global_assignment(node->children[0]->entry);
        break;
    case SYM_PARAMETER:
        generate_parameter_assignment(node->children[0]->entry);
        break;
    case SYM_LOCAL_VAR:
        generate_local_assignment(node->children[0]->entry, function);
        break;
    }
}

static void generate_func_content(node_t *root, symbol_t *function, scope s)
{
    switch (root->type)
    {
    case DECLARATION_LIST:
    {
        break;
    }
    case ASSIGNMENT_STATEMENT:
    {
        return generate_assignment(root, function, s);
    }
    case IDENTIFIER_DATA:
    {
        if (root->entry != NULL)
        {
            switch (root->entry->type)
            {
            case SYM_LOCAL_VAR:
            case SYM_PARAMETER:
            case SYM_GLOBAL_VAR:
            {
                return generate_access(root->entry, function);
            }
            }
        }
        break;
    }
    default:
    {
        for (int i = 0; i < root->n_children; i++)
        {
            if (root->children[i] != NULL)
                generate_func_content(root->children[i], function, s);
        }
        break;
    }
    }
}

static void generate_function(symbol_t *symbol)
{
    printf(".globl __vslc_%s\n", symbol->name);
    puts(".text");
    printf("__vslc_%s:\n", symbol->name);
    symbol_t **asd = malloc(symbol->nparms * sizeof(symbol_t *));
    tlhash_values(symbol->locals, asd);
    free(asd);
    scope s;
    s.if_id = 0;
    s.while_id = 0;
    generate_func_content(symbol->node, symbol, s);
    puts("");
}

static void generate_functions(void)
{
    size_t gname_size = tlhash_size(global_names);
    symbol_t **gnames = malloc(gname_size * sizeof(symbol_t *));
    tlhash_values(global_names, (void **)gnames);
    for (int i = 0; i < tlhash_size(global_names); i++)
    {
        symbol_t *curr_sym = gnames[i];
        if (curr_sym->type == SYM_FUNCTION)
        {
            if (curr_sym->seq == 0)
            {
                generate_main(curr_sym);
                puts("");
            }
            generate_function(curr_sym);
        }
    }
    free(gnames);
}

void generate_program(void)
{
    generate_stringtable();
    generate_global_vars();
    generate_functions();

    // /* Put some dummy stuff to keep the skeleton from crashing */
    // puts(".globl main");
    // puts(".text");
    // puts("main:");
    // puts("\tmovq $0, %rax");
    // puts("\tcall exit");
}
