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
    puts("newline: .asciz \"\\n\"");
    puts("errout: .asciz \"Wrong number of arguments\"");

    /* TODO:  handle the strings from the program */
    for (int i = 0; i < stringc; i++)
    {
        printf("STR%d:\t.asciz %s\n", i, string_list[i]);
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
    printf("\tcall __vslc_%s\n", first->name);
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
    printf("\tmovq _%s(%%rip), %%rax\n", symbol->name);
}

// Moves the given parameter into %rax
static void generate_parameter_access(symbol_t *symbol)
{
#if DEBUG_GENERATOR == 1
    printf("# Access parameter (%s, seq: %lu) #\n", symbol->name, symbol->seq);
#endif
    // x86's push decrements before moving the value into the stack,
    // so -8(%rbp) contains the first param, -16(%rbp) the second, etc.
    int rbp_offset = -((symbol->seq + 1) * 8);
    printf("\tmovq %d(%%rbp), %%rax\n", rbp_offset);
}

// Moves the given symbol into %rax
static void generate_local_access(symbol_t *symbol, symbol_t *function)
{
#if DEBUG_GENERATOR == 1
    printf("# Access local (%s, seq: %lu) #\n", symbol->name, symbol->seq);
#endif
    int rbp_offset = -((symbol->seq + 1) * 8 + ALIGNED_VARIABLES(function->nparms));
    printf("\tmovq %d(%%rbp), %%rax\n", rbp_offset);
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
    puts("\tcmp %rax, %r10");
}

static void generate_expression(node_t *node, symbol_t *function, scope s)
{
    if (node == NULL)
    {
        return;
    }
    switch (node->type)
    {
    case IDENTIFIER_DATA:
    {
        if (node->entry != NULL && node->entry->type != SYM_FUNCTION)
            return generate_access(node->entry, function);
        break;
    }
    case NUMBER_DATA:
    {
        printf("\tmovq $%ld, %%rax\n", *(long *)node->data);
        return;
    }
    case EXPRESSION:
    {
        // Expressions with data = NULL are always function calls
        if (node->data == NULL)
        {
            return generate_function_call(node, function, s);
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
#if DEBUG_GENERATOR == 1
                printf("# Addition of %s and %s #\n", (char *)node->children[0]->data, (char *)node->children[1]->data);
#endif
                puts("\taddq %r10, %rax");
                break;
            }
            case '-':
            {
#if DEBUG_GENERATOR == 1
                printf("# Subtraction of %s by %s #\n", (char *)node->children[0]->data, (char *)node->children[1]->data);
#endif
                puts("\tsubq %rax, %r10");
                puts("\tmovq %r10, %rax");
                break;
            }
            case '*':
            {
#if DEBUG_GENERATOR == 1
                printf("# Multiplication of %s by %s #\n", (char *)node->children[0]->data, (char *)node->children[1]->data);
#endif
                puts("\timulq %r10");
                break;
            }
            case '/':
            {
#if DEBUG_GENERATOR == 1
                printf("# Division of %s by %s #\n", (char *)node->children[0]->data, (char *)node->children[1]->data);
#endif
                puts("\tmovq %rax, %rdx");
                puts("\tmovq %r10, %rax");
                puts("\tmovq %rdx, %r10");
                puts("\tcqto"); //Extend sign from %rax into %rdx.
                puts("\tidivq %r10");
                break;
            }
            case '<':
            {
#if DEBUG_GENERATOR == 1
                printf("# Bitwise left shift of %s by %s #\n", (char *)node->children[0]->data, (char *)node->children[1]->data);
#endif
                puts("\tmovq %rax, %rcx");
                puts("\tmovq %r10, %rax");
                puts("\tshl %cl, %rax");
                break;
            }
            case '>':
            {
#if DEBUG_GENERATOR == 1
                printf("# Bitwise right shift of %s by %s #\n", (char *)node->children[0]->data, (char *)node->children[1]->data);
#endif
                puts("\tmovq %rax, %rcx");
                puts("\tmovq %r10, %rax");
                puts("\tshr %cl, %rax");
                break;
            }
            case '&':
            {
#if DEBUG_GENERATOR == 1
                printf("# Bitwise and of %s and %s #\n", (char *)node->children[0]->data, (char *)node->children[1]->data);
#endif
                puts("\tand %r10, %rax");
                break;
            }
            case '|':
            {
#if DEBUG_GENERATOR == 1
                printf("# Bitwise or of %s and %s #\n", (char *)node->children[0]->data, (char *)node->children[1]->data);
#endif
                puts("\tor %r10, %rax");
                break;
            }
            case '^':
            {
#if DEBUG_GENERATOR == 1
                printf("# Bitwise xor of %s and %s #\n", (char *)node->children[0]->data, (char *)node->children[1]->data);
#endif
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
#if DEBUG_GENERATOR == 1
                printf("# Unary negation of %s #\n", (char *)node->children[0]->data);
#endif
                puts("\tneg %rax");
                break;
            }
            case '~':
            {
#if DEBUG_GENERATOR == 1
                printf("# Unary bitwise not of %s #\n", (char *)node->children[0]->data);
#endif
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
    printf("\tmovq %%rax, _%s(%%rip)\n", symbol->name);
}

static void generate_parameter_assignment(symbol_t *symbol)
{
#if DEBUG_GENERATOR == 1
    printf("# Parameter assignment of %s #\n", symbol->name);
#endif
    int rbp_offset = -((symbol->seq + 1) * 8);
    printf("\tmovq %%rax, %d(%%rbp)\n", rbp_offset);
}

static void generate_local_assignment(symbol_t *symbol, symbol_t *function)
{
#if DEBUG_GENERATOR == 1
    printf("# Local assignment of %s #\n", symbol->name);
#endif
    int rbp_offset = -((symbol->seq + 1) * 8 + ALIGNED_VARIABLES(function->nparms));
    printf("\tmovq %%rax, %d(%%rbp)\n", rbp_offset);
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

static void generate_print_statement(node_t *root, symbol_t *function, scope s)
{

    for (int i = 0; i < root->n_children; i++)
    {
        node_t *child = root->children[i];
        switch (child->type)
        {
        case STRING_DATA:
        {
#if DEBUG_GENERATOR == 1
            puts("# Loading string from data #");
#endif
            puts("\tlea strout(%rip), %rdi");
            printf("\tlea STR%ld(%%rip), %%rsi\n", *(size_t *)child->data);
            break;
        }
        case IDENTIFIER_DATA:
        case NUMBER_DATA:
        case EXPRESSION:
        {
#if DEBUG_GENERATOR == 1
        printf("# Evaluating expression before print #\n");
#endif
            generate_expression(child, function, s);
            puts("\tlea intout(%rip), %rdi");
            puts("\tmovq %rax, %rsi");
            break;
        }
        }
#if DEBUG_GENERATOR == 1
        printf("# Printing statement %d/%lu #\n", i, root->n_children);
#endif
        puts("\tpushq %rax");
        puts("\tmovq $0, %rax");
        puts("\tcall printf");
        puts("\tpopq %rax");
    };
#if DEBUG_GENERATOR == 1
    puts("# Newline at end of print statement #");
#endif
    puts("\tlea newline(%rip), %rdi");
    puts("\tpushq %rax");
    puts("\tmovq $0, %rax");
    puts("\tcall printf");
    puts("\tpopq %rax");
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
    case PRINT_STATEMENT:
    {
        return generate_print_statement(root, function, s);
    }
    case RETURN_STATEMENT:
    {
        if (root->n_children > 0)
        {
            generate_expression(root->children[0], function, s);
            puts("\tleave");
            puts("\tret");
            return;
        }
        return;
    }
    case IF_STATEMENT:
    {
        return generate_if_statement(root, function, s);
    }
    case WHILE_STATEMENT:
    {
        return generate_while_statement(root, function, s);
    }
    case NULL_STATEMENT:
    { // Why is continue called a NULL statement?
        printf("\tjmp __vslwhile_%d_top\n", s.while_id);
        return;
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

// Comparison function between two symbol table entries. Sorts by sequence number
int seq_comp(const void *e1, const void *e2)
{
    if (((symbol_t *)e1)->seq > ((symbol_t *)e2)->seq)
        return 1;
    if (((symbol_t *)e1)->seq < ((symbol_t *)e2)->seq)
        return -1;
    return 0;
}

static void generate_function(symbol_t *symbol)
{
    printf(".globl __vslc_%s\n", symbol->name);
    puts(".text");
    printf("__vslc_%s:\n", symbol->name);

    // Push the basepointer so we can use the stack dynamically.
    // The stack pointer is stored in the base pointer from the mov-instruction above, so this practically stores the old stack frame
    puts("\tpushq %rbp");
    // Move the current stack pointer into the base pointer register before we allocate space on the stack
    puts("\tmovq %rsp, %rbp");

    size_t nlocals = tlhash_size(symbol->locals);
    // Push all function arguments to the bottom of the stack in reverse order
    symbol_t **locals = (symbol_t **) malloc(sizeof(symbol_t *) * nlocals);
    tlhash_values(symbol->locals, (void **) locals);
    // Sort to ensure correct pushing order
    qsort(locals, nlocals, sizeof(symbol_t *), seq_comp);

    int status;
    for (int argn = 0; argn < symbol->nparms; argn++)
    {
#if DEBUG_GENERATOR == 1
        printf("# Push argument %d to function stack frame #\n", argn);
#endif
        if (argn <= 5)
        {
            printf("\tpushq %s\n", record[argn]);
        }
        else if(symbol->nparms > 6)
        {
            // Which arg this is relative to the register-loaded arguments, starting at 0
            // I.e. if this is arg #7, it's relative seq is 0, if it's #8, the relative seq is 1, and so on
            // This tells us how far back to go on the stack from where the stack arguments are
            int relative_seq = argn - 7;

            // The rest of the arguments are stored on the stack before the return address
            // Initial offset:
            // +  8 bytes to leave current stack frame
            // +  8 bytes to skip %rbp
            // +  8 bytes to skip return address
            // = 24 byte
            // Then we go back 8 bytes for each argument
            int sp_offset = 24 + (relative_seq * 8);
#if DEBUG_GENERATOR == 1
            printf("# Retrieve argument %d from preceding stack frame #\n", argn);
#endif
            // %rdi has already been pushed to the stack and is safe to use
            printf("\tmovq %d(%%rbp), %%rdi\n", sp_offset);
            puts("\tpushq %rdi");
        }
    }
    free(locals);

    // Allocate the function's stack frame and align the stack pointer to a 16-byte boundary
    // The function call pushes the return address (8 bytes), and we need 8 bytes for each local variable
    // So the SP needs to be aligned by allocating 8 more bytes if nlocals is an even number
    size_t stack_frame_size = (nlocals % 2 == 1) ? nlocals * 8 : nlocals * 8 + 8;

#if DEBUG_GENERATOR == 1
    printf("# Allocate %lu bytes on the stack for %lu locals (aligned: %s) #\n",
        stack_frame_size,
        nlocals,
        (nlocals % 2 == 1) ? "no" : "yes"
    );
#endif
    printf("\tsubq $%lu, %%rsp\n", stack_frame_size);

#if DEBUG_GENERATOR == 1
    printf("# Function body (%s) #\n", symbol->name);
#endif
    // Setup function scope for if and while labels and generate the meat & potatoes of the function
    scope s;
    s.if_id = 0;
    s.while_id = 0;
    generate_statements(symbol->node, symbol, s);

    // The leave instruction restores the stack for us by setting %rsp = %rbp and popping into %rbp
    puts("\tleave");
    puts("\tret");
}

// Takes a calling expression and generates code to call the function from a given caller
static void generate_function_call(node_t *call_node, symbol_t *caller, scope s)
{
    // Identifier node for the function to be called
    node_t *func_identifier = call_node->children[0];
    // Expression list for the function arguments
    node_t *arg_list = call_node->children[1];

#if DEBUG_GENERATOR == 1
    printf("# Function call (%s) #\n", (char *)func_identifier->data);
#endif

    // If the arglist is null the function takes no parameters
    if (arg_list != NULL)
    {
        // Reverse order because args should be pushed onto the stack in reverse order
        for (int argn = arg_list->n_children - 1; argn >= 0; argn--)
        {
            // Generate code that resolves the value of the argument
#if DEBUG_GENERATOR == 1
            printf("# Resolve value of argument %d #\n", argn);
#endif
            generate_expression(arg_list->children[argn], caller, s);

            // First 6 arguments go into registers
            if (argn <= 5)
            {
                const char *param_register = record[argn];
                printf("\tmovq %%rax, %s\n", param_register);
            }
            // Remaining args go to the stack
            else if (argn > 5)
            {
                puts("\tpushq %rax");
            }
        }
    }

    // Perform the call
    symbol_t *function = func_identifier->entry;
    printf("\tcall __vslc_%s\n", function->name);
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
