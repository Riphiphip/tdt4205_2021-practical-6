#pragma once

typedef struct
{
    int if_id;
    int while_id;
} scope;

#define DEBUG_GENERATOR 1

// How many registers are used for parameters before resorting to using the stack
#define N_PARAM_REGISTERS 6
static const char *record[6] = {"%rdi", "%rsi", "%rdx", "%rcx", "%r8", "%r9"};
static const char *callee_save_registers[6] = {"%rbx", "%r12", "%r13", "%r14", "%r15", "%rbp"};

#define ALIGN_BYTES(amount) ((amount + 15) & (~15))
#define ALIGNED_VARIABLES(amount) (ALIGN_BYTES(amount*8))

static void generate_global_access(symbol_t *symbol);
static void generate_parameter_access(symbol_t *symbol);
static void generate_local_access(symbol_t *symbol, symbol_t* function);
static void generate_access(symbol_t *symbol, symbol_t* function);

static void generate_global_assignment(symbol_t *symbol);
static void generate_parameter_assignment(symbol_t *symbol);
static void generate_local_assignment(symbol_t *symbol, symbol_t* function);
static void generate_assignment(node_t *node, symbol_t* function, scope s);

static void generate_expression(node_t *node, symbol_t* function, scope s);
static void generate_comparison(node_t *node, symbol_t* function, scope s);

static void generate_statements(node_t *node, symbol_t* function, scope s);

static void generate_function(symbol_t *symbol);
static void generate_function_call(node_t *call_node, symbol_t *caller, scope s);