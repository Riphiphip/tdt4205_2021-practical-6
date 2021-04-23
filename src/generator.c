#include "vslc.h"
#include "generator.h"

#define MIN(a, b) (((a) < (b)) ? (a) : (b))

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
    for (int i = 0; i < gname_size; i++)
    {
        symbol_t *curr_sym = gnames[i];
        if (curr_sym->type == SYM_GLOBAL_VAR)
        {
            printf("_%s:\t.zero 8\n", curr_sym->name);
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

static void generate_comparison(node_t *root, symbol_t *function, scope s)
{
    generate_expression(root->children[0], function, s);
    puts("\tpushq %rax");
    generate_expression(root->children[1], function, s);
    puts("\tpopq %r10");
    puts("\tcmp %r10, %rax");
}

static void generate_function_call(node_t *node, symbol_t *function)
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
    case EXPRESSION:
    {
        if (node->data == NULL)
        {
            return generate_function_call(node, function);
        }
        generate_expression(node->children[0], function, s);
        if (node->n_children > 1)
        {
            puts("\tpushq %rax");
            generate_expression(node->children[1], function, s);
            puts("\tpopq %r10");
            switch (*(char *)node->data)
            {
            case '+':
            {
                puts("\taddq %r10, %rax");
                break;
            }
            case '-':
            {
                puts("\tsubq %rax, %r10");
                puts("\tmovq %r10, %rax");
                break;
            }
            case '*':
            {
                puts("\tpushq %rdx");
                puts("\timulq %r10");
                puts("\tpopq %rdx");
                break;
            }
            case '/':
            {
                puts("\tpushq %rdx");
                puts("\tmovq %rax, %rdx");
                puts("\tmovq %r10, %rax");
                puts("\tmovq %rdx, %r10");
                puts("\tcqto"); //Extend sign from %rax into %rdx.
                puts("\tidivq %r10");
                puts("\tpopq %rdx");
                break;
            }
            case '<':
            {
                puts("\tpushq %rcx");
                puts("\tmovq %rax, %rcx");
                puts("\tmovq %r10, %rax");
                puts("\tshl %cl, %rax");
                puts("\tpopq %rcx");
                break;
            }
            case '>':
            {
                puts("\tpushq %rcx");
                puts("\tmovq %rax, %rcx");
                puts("\tmovq %r10, %rax");
                puts("\tshr %cl, %rax");
                puts("\tpopq %rcx");
                break;
            }
            case '&':
            {
                puts("\tand %r10, %rax");
                break;
            }
            case '|':
            {
                puts("\tor %r10, %rax");
                break;
            }
            case '^':
            {
                puts("\txor %r10, %rax");
                break;
            }
            }
        }
        else
        {
            switch (*(char *)node->data)
            {
            case '-':
            {
                puts("\tneg %rax");
                break;
            }
            case '~':
            {
                puts("\tnot %rax");
            }
            }
        }
        break;
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

static void generate_if_statement(node_t *root, symbol_t *function, scope s)
{
    s.if_id = ++if_id;
    printf("__vslif_%d_top:\n", s.if_id);
    generate_comparison(root->children[0], function, s);
    char *jmp_instr;

    switch (*(char *)(root->children[0]->data))
    {
    case '=':
    {
        jmp_instr = "jne";
        break;
    }
    case '<':
    {
        jmp_instr = "jnl";
        break;
    }
    case '>':
    {
        jmp_instr = "jng";
        break;
    }
    }
    if (root->n_children == 2)
    {
        // No Else
        printf("\t%s __vslif_%d_bottom\n", jmp_instr, s.if_id);
        generate_statements(root->children[1], function, s);
    }
    else if (root->n_children > 2)
    {
        // With Else
        printf("\t%s __vslif_%d_else\n", jmp_instr, s.if_id);
        generate_statements(root->children[1], function, s);
        printf("\tjmp __vslif_%d_bottom\n", s.if_id);
        printf("__vslif_%d_else:\n", s.if_id);
        generate_statements(root->children[2], function, s);
    }
    printf("__vslif_%d_bottom:\n", s.if_id);
}

static void generate_while_statement(node_t *root, symbol_t *function, scope s)
{
    s.while_id = ++while_id;
    printf("__vslwhile_%d_top:\n", s.while_id);
    generate_comparison(root->children[0], function, s);
    char *jmp_instr;

    switch (*(char *)(root->children[0]->data))
    {
    case '=':
    {
        jmp_instr = "jne";
        break;
    }
    case '<':
    {
        jmp_instr = "jnl";
        break;
    }
    case '>':
    {
        jmp_instr = "jng";
        break;
    }
    }
    // No Else
    printf("\t%s __vslwhile_%d_bottom\n", jmp_instr, s.while_id);
    generate_statements(root->children[1], function, s);
    printf("\tjmp __vslwhile_%d_top\n", s.while_id);
    printf("__vslwhile_%d_bottom:\n", s.while_id);
}

static void generate_statements(node_t *root, symbol_t *function, scope s)
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
    case RETURN_STATEMENT:
    {
        if (root->n_children > 0)
        {
            generate_expression(root->children[0], function, s);
            puts("\tret");
            return;
        }
        return;
    }
    case IF_STATEMENT:
    {
        return generate_if_statement(root, function, s);
    }
    case WHILE_STATEMENT:{
        return generate_while_statement(root, function, s);
    }
    default:
    {
        for (int i = 0; i < root->n_children; i++)
        {
            if (root->children[i] != NULL)
                generate_statements(root->children[i], function, s);
        }
        break;
    }
    }
}

static void generate_function(symbol_t *symbol)
{
    printf(".globl __vsl_%s\n", symbol->name);
    puts(".text");
    printf("__vslc_%s:\n", symbol->name);

    // Move the current stack pointer into the base pointer register
    // This is done so we can restore it later
    puts("movq %rsp, %rbp");
    // Fix stack allignment from function calls
    puts("subq 8, %rsp");

    // Push the basepointer so we can use the stack dynamically.
    // The stack pointer is stored in the base pointer from the mov-instruction above, so this practically pushes rsp too
    puts("push %rbp");

    // 64 bits/8 bytes for each var on the stack
    size_t stack_allocation = 8 * tlhash_size(symbol->locals);
    // Allocate space on the stack for all locals
    printf("subq $%lu, %rsp", stack_allocation);
    // Then put copies of the args onto the stack
    // size_t nlocals = tlhash_size(symbol->locals);
    // char **local_keys = malloc(sizeof(char *) * nlocals);
    // tlhash_keys(symbol->locals, local_keys);
    // int status;
    // for (int arg = 0; arg < symbol->nparms; arg++)
    // {
    //     char *key = local_keys[arg];
    //     symbol_t *local_sym;
    //     tlhash_lookup(symbol->locals, key, strlen(key) + 1, local_sym);
    //     node_t *local_node = local_sym->node;
    //     int64_t val = (int64_t) local_node->data;
    //     printf("pushq $%ld", val);
    // }
    // free(local_keys);

    // symbol_t **asd = malloc(symbol->nparms * sizeof(symbol_t *));
    // tlhash_values(symbol->locals, asd);
    // free(asd);
    scope s;
    s.if_id = 0;
    s.while_id = 0;
    generate_func_content(symbol->node, symbol, s);

    // Restore the stack pointer and pop the basepointer
    // This also deallocs what space we used on the stack, regardless of how much was allocated
    // This means we avoid popping args off the stack too
    puts("movq %rbp, %rsp");
    printf("pop %rbp");
    puts("ret");
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
