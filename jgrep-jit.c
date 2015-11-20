#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <libgccjit.h>

static void die(const char* c)
{
  fprintf(stderr, "ERROR: %s\n", c);
  exit(EXIT_FAILURE);
}

#if 0
static int matchstar(int c, const char *regexp, const char *text);

/* matchhere: search for regexp at beginning of text */
static int matchhere(const char *regexp, const char *text)
{
    if (regexp[0] == '\0')
        return 1;
    if (regexp[1] == '*')
        return matchstar(regexp[0], regexp+2, text);
    if (regexp[0] == '$' && regexp[1] == '\0')
        return *text == '\0';
    if (*text!='\0' && (regexp[0]=='.' || regexp[0]==*text))
        return matchhere(regexp+1, text+1);
    return 0;
}

/* matchstar: search for c*regexp at beginning of text */
static int matchstar(int c, const char *regexp, const char *text)
{
    do {    /* a * matches zero or more instances */
        if (matchhere(regexp, text))
            return 1;
    } while (*text != '\0' && (*text++ == c || c == '.'));
    return 0;

}

/* match: search for regexp anywhere in text */
static int match(const char *regexp, const char *text)
{
    if (regexp[0] == '^')
        return matchhere(regexp+1, text);
    do {    /* must look even if string is empty */
        if (matchhere(regexp, text))
            return 1;
    } while (*text++ != '\0');
    return 0;
}
#endif

static const char* new_block_name(void)
{
  static int n = 0;
  enum { SIZE = 16 };
  static char c[SIZE];

  snprintf(c, SIZE, "block-%02d", n);
  c[SIZE-1] = '\0';

  n++;

  return c;
}

static const char* new_function_name(void)
{
  static int n = 0;
  enum { SIZE = 16 };
  static char c[SIZE];

  snprintf(c, SIZE, "matchhere_%d", n);
  c[SIZE-1] = '\0';

  n++;

  return c;
}

static const char* new_local_name(void)
{
  static int n = 0;
  enum { SIZE = 16 };
  static char c[SIZE];

  snprintf(c, SIZE, "tmp_%d", n);
  c[SIZE-1] = '\0';

  n++;

  return c;
}

static void generate_return_zero(gcc_jit_context* ctx, gcc_jit_function *fun, gcc_jit_block** return_zero)
{
  if (*return_zero == NULL)
  {
    gcc_jit_type *int_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_INT);
    *return_zero = gcc_jit_function_new_block(fun, new_block_name());
    gcc_jit_block_end_with_return(
        *return_zero, /* loc */ NULL,
        gcc_jit_context_zero(ctx, int_type));
  }
}

static gcc_jit_function *generate_code_matchhere(gcc_jit_context *ctx, const char* regexp, const char* function_name)
{
  gcc_jit_type *int_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_INT);
  gcc_jit_type *char_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_CHAR);
  gcc_jit_type *const_char_ptr_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_CONST_CHAR_PTR);

  gcc_jit_param *param_text = gcc_jit_context_new_param(ctx, /* loc */ NULL, const_char_ptr_type, "text");
  gcc_jit_rvalue *rval_text = gcc_jit_param_as_rvalue(param_text);

  // matchhere
  gcc_jit_param* params[] = { param_text };
  gcc_jit_function *matchhere = gcc_jit_context_new_function(ctx, /* loc */ NULL,
      GCC_JIT_FUNCTION_INTERNAL, int_type, function_name,
      1, params, /* is_variadic */ 0);
  gcc_jit_block* current_block = gcc_jit_function_new_block(matchhere, new_block_name());

  gcc_jit_rvalue* text_plus_one = 
    gcc_jit_context_new_cast(
        ctx, /* loc */ NULL,
        gcc_jit_lvalue_get_address(
          gcc_jit_context_new_array_access(
            ctx, /* loc */ NULL,
            rval_text,
            gcc_jit_context_one(ctx, int_type)),
          /* loc */ NULL),
        const_char_ptr_type);

  gcc_jit_block* return_zero = NULL;

  gcc_jit_block* return_one = gcc_jit_function_new_block(matchhere, new_block_name());
  gcc_jit_block_end_with_return(
      return_one, /* loc */ NULL,
      gcc_jit_context_one(ctx, int_type));

  // Now go creating
  for (;;)
  {
    if (regexp[0] == '\0')
    {
      gcc_jit_block_end_with_jump(
          current_block, /* loc */ NULL,
          return_one);
      break; // We are done
    }
    else if (regexp[1] == '*')
    {
      // Generate code for the remaining regular expression
      gcc_jit_function *remaining_regexp_match = generate_code_matchhere(ctx, regexp + 2, new_function_name());

      gcc_jit_block* loop_body = gcc_jit_function_new_block(matchhere, new_block_name());
      gcc_jit_block* loop_check = gcc_jit_function_new_block(matchhere, new_block_name());

      gcc_jit_block_end_with_jump(current_block, /* loc */ NULL, loop_body);

      gcc_jit_rvalue* args[] = { rval_text };
      gcc_jit_rvalue* match_remainder = 
        gcc_jit_context_new_comparison(
            ctx, /* loc */ NULL,
            GCC_JIT_COMPARISON_NE,
            gcc_jit_context_new_call(ctx, /* loc */ NULL,
              remaining_regexp_match, 1, args),
            gcc_jit_context_zero(ctx, int_type));

      gcc_jit_block_end_with_conditional(loop_body, /* loc */ NULL,
          match_remainder,
          return_one,
          loop_check);

      gcc_jit_lvalue* tmp = gcc_jit_function_new_local(matchhere, /* loc */ NULL, char_type, new_local_name());
      gcc_jit_block_add_assignment(
          loop_check, /* loc */ NULL,
          tmp,
          gcc_jit_lvalue_as_rvalue(
            gcc_jit_rvalue_dereference(rval_text, /* loc */ NULL)));

      gcc_jit_rvalue* check_expr;
      if (regexp[0] == '.')
      {
        check_expr =
          gcc_jit_context_new_comparison(ctx, /* loc */ NULL,
              GCC_JIT_COMPARISON_NE,
              gcc_jit_lvalue_as_rvalue(tmp),
              gcc_jit_context_zero(ctx, char_type));
      }
      else
      {
        check_expr =
          gcc_jit_context_new_comparison(ctx, /* loc */ NULL,
              GCC_JIT_COMPARISON_EQ,
              gcc_jit_lvalue_as_rvalue(tmp),
              gcc_jit_context_new_rvalue_from_int(ctx, char_type, regexp[0]));
      }

      generate_return_zero(ctx, matchhere, &return_zero);

      gcc_jit_block_add_assignment(loop_check, /* loc */ NULL,
          gcc_jit_param_as_lvalue(param_text),
          text_plus_one);
      gcc_jit_block_end_with_conditional(loop_check, /* loc */ NULL,
          check_expr,
          loop_body,
          return_zero);

      break; // We are done
    }
    else if (regexp[0] == '$' && regexp[1] == '\0')
    {
      gcc_jit_block_end_with_return(
          current_block, /* loc */ NULL,
          gcc_jit_context_new_comparison(ctx, /* loc */ NULL,
            GCC_JIT_COMPARISON_EQ, 
            gcc_jit_lvalue_as_rvalue(
              gcc_jit_rvalue_dereference(rval_text, /* loc */ NULL)
              ),
            gcc_jit_context_zero(ctx, char_type)));

      regexp++;
    }
    else if (regexp[0] == '.')
    {
      generate_return_zero(ctx, matchhere, &return_zero);

      gcc_jit_block* next_block = gcc_jit_function_new_block(matchhere, new_block_name());

      // if (*text == '\0')
      //    return 0;
      gcc_jit_block_end_with_conditional(
          current_block, /* loc */ NULL,
          gcc_jit_context_new_comparison(ctx, /* loc */ NULL,
            GCC_JIT_COMPARISON_EQ, 
            gcc_jit_lvalue_as_rvalue(
              gcc_jit_rvalue_dereference(rval_text, /* loc */ NULL)
              ),
            gcc_jit_context_zero(ctx, char_type)),
          return_zero,
          next_block);

      // text = &text[1]; // pointer arithmetic
      gcc_jit_block_add_assignment(next_block, /* loc */ NULL,
          gcc_jit_param_as_lvalue(param_text),
          text_plus_one);

      // Chain the code
      current_block = next_block;

      // Done with the current letter
      regexp++;
    }
    else
    {
      generate_return_zero(ctx, matchhere, &return_zero);

      gcc_jit_block* next_block = gcc_jit_function_new_block(matchhere, new_block_name());

      // if (*text != regexp[0])
      //    return 0;
      gcc_jit_block_end_with_conditional(
          current_block, /* loc */ NULL,
          gcc_jit_context_new_comparison(ctx, /* loc */ NULL,
            GCC_JIT_COMPARISON_NE, 
            gcc_jit_lvalue_as_rvalue(
              gcc_jit_rvalue_dereference(rval_text, /* loc */ NULL)
              ),
            gcc_jit_context_new_rvalue_from_int(ctx, char_type, regexp[0])),
          return_zero,
          next_block);

      // text = &text[1]; // pointer arithmetic
      gcc_jit_block_add_assignment(next_block, /* loc */ NULL,
          gcc_jit_param_as_lvalue(param_text),
          text_plus_one);

      // Chain the code
      current_block = next_block;

      // Done with the current letter
      regexp++;
    }
  }

  // char c[64];
  // snprintf(c, 63, "%s.dot", function_name);
  // c[63] = '\0';

  // gcc_jit_function_dump_to_dot(matchhere, c);

  return matchhere;
}

void generate_code_regexp(gcc_jit_context *ctx, const char* regexp)
{
  const char* matchhere_regexp = regexp;
  if (regexp[0] == '^')
  {
    matchhere_regexp++;
  }
  gcc_jit_function* matchhere = generate_code_matchhere(ctx, matchhere_regexp, "matchhere");

  // match function
  gcc_jit_type *int_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_INT);
  gcc_jit_type *char_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_CHAR);
  gcc_jit_type *const_char_ptr_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_CONST_CHAR_PTR);

  gcc_jit_param *param_text = gcc_jit_context_new_param(ctx, /* loc */ NULL, const_char_ptr_type, "text");
  gcc_jit_rvalue *rval_text = gcc_jit_param_as_rvalue(param_text);

  gcc_jit_param* params[] = { param_text };
  gcc_jit_function *match = gcc_jit_context_new_function(ctx, /* loc */ NULL,
      GCC_JIT_FUNCTION_EXPORTED, int_type, "match",
      1, params, /* is_variadic */ 0);

  gcc_jit_rvalue* args[] = { rval_text };
  gcc_jit_rvalue* call_to_matchhere = gcc_jit_context_new_call(ctx, /* loc */ NULL,
      matchhere,
      1, args);
  if (regexp[0] == '^')
  {
    gcc_jit_block* block = gcc_jit_function_new_block(match, new_block_name());

    gcc_jit_block_end_with_return(
        block, /* loc */ NULL,
        gcc_jit_context_new_comparison(ctx, /* loc */ NULL,
          GCC_JIT_COMPARISON_NE, 
          call_to_matchhere,
          gcc_jit_context_zero(ctx, int_type)));
  }
  else
  {
    gcc_jit_block* loop_body = gcc_jit_function_new_block(match, new_block_name());
    gcc_jit_block* return_one = gcc_jit_function_new_block(match, new_block_name());
    gcc_jit_block* condition_check = gcc_jit_function_new_block(match, new_block_name());
    gcc_jit_block* return_zero = gcc_jit_function_new_block(match, new_block_name());

    gcc_jit_block_end_with_conditional(
        loop_body, /* loc */ NULL,
        gcc_jit_context_new_comparison(ctx, /* loc */ NULL,
          GCC_JIT_COMPARISON_NE, 
          call_to_matchhere,
          gcc_jit_context_zero(ctx, int_type)),
        return_one,
        condition_check);

    gcc_jit_block_end_with_return(
        return_one, /* loc */ NULL,
        gcc_jit_context_one(ctx, int_type));

    gcc_jit_lvalue* tmp = gcc_jit_function_new_local(match, /* loc */ NULL, char_type, new_local_name());
    gcc_jit_block_add_assignment(
        condition_check, /* loc */ NULL,
        tmp,
        gcc_jit_lvalue_as_rvalue(
          gcc_jit_rvalue_dereference(rval_text, /* loc */ NULL)));
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
    gcc_jit_block_end_with_conditional(
        condition_check, /* loc */ NULL,
        gcc_jit_context_new_comparison(ctx, /* loc */ NULL,
          GCC_JIT_COMPARISON_NE, 
          gcc_jit_lvalue_as_rvalue(tmp),
          gcc_jit_context_zero(ctx, char_type)),
        loop_body,
        return_zero);

    gcc_jit_block_end_with_return(
        return_zero, /* loc */ NULL,
        gcc_jit_context_zero(ctx, int_type));
  }
  gcc_jit_function_dump_to_dot(match, "match.dot");

  // gcc_jit_context_dump_to_file(ctx, "generated-regex.dump", /* update-locations */ 1);
  // gcc_jit_context_set_bool_option(ctx, GCC_JIT_BOOL_OPTION_DEBUGINFO, 1);
}

int main(int argc, char *argv[])
{
  if (argc != 3)
  {
    fprintf(stderr, "usage: %s regex filename\n", argv[0]);
    exit(EXIT_FAILURE);
  }

  const char* regexp = argv[1];

  gcc_jit_context *ctx;
  ctx = gcc_jit_context_acquire ();
  if (ctx == NULL)
    die("acquired context is NULL");

  gcc_jit_context_set_int_option(ctx, GCC_JIT_INT_OPTION_OPTIMIZATION_LEVEL, 2);

  generate_code_regexp(ctx, regexp);

  gcc_jit_result *result = gcc_jit_context_compile(ctx);
  if (result == NULL)
    die("compilation failed");

  void *function_addr = gcc_jit_result_get_code(result, "match");
  if (function_addr == NULL)
    die("error getting 'match'");

  typedef int (*match_fun_t)(const char*);
  match_fun_t match = (match_fun_t)function_addr;

  FILE *f = fopen(argv[2], "r");
  if (f == NULL)
  {
    fprintf(stderr, "error opening file '%s': %s\n",
        argv[2],
        strerror(errno));
    exit(EXIT_FAILURE);
  }

  char* line = NULL;
  size_t length = 0;

  while (getline(&line, &length, f) != -1)
  {
    if (match(line))
      fprintf(stdout, "%s", line);
  }

  free(line);
  fclose(f);

  return 0;
}
