#pragma once

typedef struct
{
    int if_id;
    int while_id;
} scope;

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