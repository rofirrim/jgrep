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

  gcc_jit_type *int_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_INT);

  // 'int n' as a parameter
  gcc_jit_param *param_n = gcc_jit_context_new_param(ctx, /* loc */ NULL, int_type, "n");
  gcc_jit_rvalue *rval_n = gcc_jit_param_as_rvalue(param_n);

  // int sum(int n)
  gcc_jit_param* params[] = { param_n };
  gcc_jit_function *sum_function = gcc_jit_context_new_function(ctx, /* loc */ NULL,
      GCC_JIT_FUNCTION_EXPORTED, int_type, "sum",
      sizeof(params)/sizeof(*params), params, /* is_variadic */ 0);

  // Local variables
  //   int s;
  gcc_jit_lvalue* sum_var = gcc_jit_function_new_local(sum_function, /* loc */ NULL, int_type, "s");
  gcc_jit_rvalue* rval_sum = gcc_jit_lvalue_as_rvalue(sum_var);
  //   int i;
  gcc_jit_lvalue* ind_var = gcc_jit_function_new_local(sum_function, /* loc */ NULL, int_type, "i");
  gcc_jit_rvalue* rval_ind = gcc_jit_lvalue_as_rvalue(ind_var);

  //       init
  //        |
  //        V
  // .-- loop_header <--.
  // |      |           |
  // |      V           |
  // |   loop_body------'
  // |
  // `--> return
  //
  gcc_jit_block *init_block = gcc_jit_function_new_block(sum_function, "init");
  gcc_jit_block *loop_header_block = gcc_jit_function_new_block(sum_function, "loop_header");
  gcc_jit_block *loop_body_block = gcc_jit_function_new_block(sum_function, "loop_body");
  gcc_jit_block *return_block = gcc_jit_function_new_block(sum_function, "return");

  // Block --- init
  //    sum = 0
  gcc_jit_block_add_assignment(init_block, /* loc */ NULL,
          sum_var,
          gcc_jit_context_new_rvalue_from_int(ctx, int_type, 0));
  //    init = 0
  gcc_jit_block_add_assignment(init_block, /* loc */ NULL,
          ind_var,
          gcc_jit_context_new_rvalue_from_int(ctx, int_type, 0));
  //    goto loop_header
  gcc_jit_block_end_with_jump(init_block, /* loc */ NULL, loop_header_block);

  // Block --- loop_header
  //    if (i < n)
  //       goto loop_body;
  //    else
  //       goto return;
  gcc_jit_block_end_with_conditional(
          loop_header_block, /* loc */ NULL,
           gcc_jit_context_new_comparison(
               ctx, /* loc */ NULL,
               GCC_JIT_COMPARISON_LT, 
               rval_ind,
               rval_n),
           loop_body_block,
           return_block);

  // Block --- loop_body
  //   s += i;
  gcc_jit_block_add_assignment_op (
          loop_body_block, /* loc */ NULL,
          sum_var,
          GCC_JIT_BINARY_OP_PLUS,
          rval_ind);
  //   i += 1;
  gcc_jit_block_add_assignment_op (
          loop_body_block, /* loc */ NULL,
          ind_var,
          GCC_JIT_BINARY_OP_PLUS,
          gcc_jit_context_one(ctx, int_type)); 
  //   goto loop_header
  gcc_jit_block_end_with_jump(
          loop_body_block, /* loc */ NULL,
          loop_header_block);

  // Block --- return
  //   return sum;
  gcc_jit_block_end_with_return(return_block, /* loc */ NULL,
          rval_sum);

  // Dump to dot
  gcc_jit_function_dump_to_dot(sum_function, "sum.dot");

  // Compile the code
  gcc_jit_result *result = gcc_jit_context_compile(ctx);
  if (result == NULL)
    die("compilation failed");

  // Test it!
  void *function_addr = gcc_jit_result_get_code(result, "sum");

  typedef int (*ptr_sum_t)(int);
  ptr_sum_t sum = (ptr_sum_t)function_addr;

  int x = sum(100);
  fprintf(stderr, "SUM(0..99) = %d\n", x);
  assert(x == 4950);

  gcc_jit_result_release(result);
  gcc_jit_context_release (ctx);

  return 0;
}
