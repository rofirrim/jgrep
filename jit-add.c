#include <libgccjit.h>

#include <stdlib.h>
#include <stdio.h>
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

  // Create code
  gcc_jit_type *int_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_INT);

  gcc_jit_param *param_a = gcc_jit_context_new_param(ctx, /* loc */ NULL, int_type, "a");
  gcc_jit_param *param_b = gcc_jit_context_new_param(ctx, /* loc */ NULL, int_type, "b");

  gcc_jit_rvalue *rval_a = gcc_jit_param_as_rvalue(param_a);
  gcc_jit_rvalue *rval_b = gcc_jit_param_as_rvalue(param_b);

  gcc_jit_rvalue *a_plus_b = gcc_jit_context_new_binary_op(ctx, /* loc */ NULL,
      GCC_JIT_BINARY_OP_PLUS, int_type, rval_a, rval_b);

  gcc_jit_param* params[2] = { param_a, param_b };
  gcc_jit_function *add_function = gcc_jit_context_new_function(ctx, /* loc */ NULL,
      GCC_JIT_FUNCTION_EXPORTED, int_type, "add",
      2, params, /* is_variadic */ 0);

  gcc_jit_block *block = gcc_jit_function_new_block(add_function, "add:top_level_block");

  gcc_jit_block_end_with_return(block, /* loc */ NULL, a_plus_b);

  // Dump to dot
  gcc_jit_function_dump_to_dot(add_function, "add.dot");

  // Compile the code
  gcc_jit_result *result = gcc_jit_context_compile(ctx);
  if (result == NULL)
    die("compilation failed");

  void *function_addr = gcc_jit_result_get_code(result, "add");

  typedef int (*ptr_add_t)(int, int);
  ptr_add_t add = (ptr_add_t)function_addr;
  int x = add(1, 2);
  assert(x == 3);

  gcc_jit_result_release(result);
  gcc_jit_context_release (ctx);

  return 0;
}
