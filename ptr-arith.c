#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#include <libgccjit.h>

// This is a reproducer for GCC bug 68370 
// https://gcc.gnu.org/bugzilla/show_bug.cgi?id=68370

int main (int argc, char **argv)
{
    gcc_jit_context *ctx;
    ctx = gcc_jit_context_acquire ();

    gcc_jit_type *int_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_INT);
    gcc_jit_type *const_char_ptr_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_CONST_CHAR_PTR);

    gcc_jit_param *param_text = gcc_jit_context_new_param(ctx, /* loc */ NULL, const_char_ptr_type, "text");
    gcc_jit_rvalue *rval_text = gcc_jit_param_as_rvalue(param_text);

    gcc_jit_param* params[] = { param_text };
    gcc_jit_function *add_function = gcc_jit_context_new_function(ctx, /* loc */ NULL,
            GCC_JIT_FUNCTION_EXPORTED, int_type, "test",
            1, params, /* is_variadic */ 0);

    gcc_jit_block *block = gcc_jit_function_new_block(add_function, "");

    // text = &text[1]; // rejected
    gcc_jit_block_add_assignment(
            block, /* loc */ NULL,
            gcc_jit_param_as_lvalue(param_text),
            gcc_jit_lvalue_get_address(
                gcc_jit_context_new_array_access(
                    ctx, /* loc */ NULL,
                    rval_text,
                    gcc_jit_context_one(ctx, int_type)),
                /* loc */ NULL));

#if 0
    // text = (const char*)&text[1]; // accepted
    gcc_jit_block_add_assignment(
        condition_check, /* loc */ NULL,
        gcc_jit_param_as_lvalue(param_text),
        gcc_jit_context_new_cast(
          ctx, /* loc */ NULL,
          gcc_jit_lvalue_get_address(
            gcc_jit_context_new_array_access(
              ctx, /* loc */ NULL,
              rval_text,
              gcc_jit_context_one(ctx, int_type)),
            /* loc */ NULL),
          const_char_ptr_type));
#endif

    gcc_jit_block_end_with_return(block, /* loc */ NULL,
            gcc_jit_context_one(ctx, int_type));

    gcc_jit_context_release(ctx);

    return 0;
}
