#include <libgccjit.h>

#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <assert.h>

void die(const char* c)
{
    fprintf(stderr, "ERROR: %s\n", c);
    exit(EXIT_FAILURE);
}

int main (int argc, char **argv)
{
    gcc_jit_context *ctx;
    ctx = gcc_jit_context_acquire ();
    if (ctx == NULL)
        die("acquired context is NULL");

    // Get bool and int types
    gcc_jit_type *bool_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_BOOL);
    gcc_jit_type *int_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_INT);

    // Create parameters: bool op, int a, int b
    gcc_jit_param *param_op = gcc_jit_context_new_param(ctx, /* loc */ NULL, bool_type, "op");
    gcc_jit_param *param_a = gcc_jit_context_new_param(ctx, /* loc */ NULL, int_type, "a");
    gcc_jit_param *param_b = gcc_jit_context_new_param(ctx, /* loc */ NULL, int_type, "b");

    // Create add_or_sub function as 'int add_or_sub(bool op, int a, int b)'
    gcc_jit_param* params[3] = { param_op, param_a, param_b };
    gcc_jit_function *add_function = gcc_jit_context_new_function(ctx, /* loc */ NULL,
            GCC_JIT_FUNCTION_EXPORTED, int_type, "add_or_sub",
            3, params, /* is_variadic */ 0);

    // Now create three blocks
    gcc_jit_block *if_block = gcc_jit_function_new_block(add_function, "if_condition");
    gcc_jit_block *true_block = gcc_jit_function_new_block(add_function, "true_block");
    gcc_jit_block *false_block = gcc_jit_function_new_block(add_function, "false_block");

    // End the first block with a
    //    if (op)
    //        goto true_block;
    //    else
    //        goto false_block"
    gcc_jit_block_end_with_conditional(
            if_block,
            /* loc */ NULL,
            gcc_jit_param_as_rvalue(param_op),
            true_block,
            false_block);

    gcc_jit_rvalue *rval_a = gcc_jit_param_as_rvalue(param_a);
    gcc_jit_rvalue *rval_b = gcc_jit_param_as_rvalue(param_b);

    // End the first block with
    //    return a + b;
    gcc_jit_block_end_with_return(
            true_block,
            /* loc */ NULL,
            gcc_jit_context_new_binary_op(ctx, /* loc */ NULL,
                GCC_JIT_BINARY_OP_PLUS, int_type,
                rval_a,
                rval_b));

    // End the second block with
    //    return a - b;
    gcc_jit_block_end_with_return(
            false_block,
            /* loc */ NULL,
            gcc_jit_context_new_binary_op(ctx, /* loc */ NULL,
                GCC_JIT_BINARY_OP_MINUS, int_type,
                rval_a,
                rval_b));

    // Dump the function to dot
    gcc_jit_function_dump_to_dot(add_function, "add_or_sub.dot");

    // Compile the code
    gcc_jit_result *result = gcc_jit_context_compile(ctx);
    if (result == NULL)
        die("compilation failed");

    void *function_addr = gcc_jit_result_get_code(result, "add_or_sub");

    typedef int (*ptr_add_or_sub_t)(bool, int, int);
    ptr_add_or_sub_t add_or_sub = (ptr_add_or_sub_t)function_addr;

    int x = add_or_sub(true, 1, 2);
    assert(x == 3);

    x = add_or_sub(false, 1, 2);
    assert(x == -1);

    gcc_jit_result_release(result);
    gcc_jit_context_release (ctx);

    return 0;
}
