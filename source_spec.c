#include "bdd-for-c.h"

#include "pe32.c"
#include "value.c"
#include "instruction.c"
#include "encoding.c"
#include "function.c"
#include "source.c"

typedef void (*fn_type_void_to_void)(void);
typedef f32 (*fn_type_void_to_f32)(void);
typedef f32 (*fn_type_f32_to_f32)(f32);
typedef f64 (*fn_type_f64_to_f64)(f64);
typedef s32 (*fn_type_void_to_s32)(void);
typedef s64 (*fn_type_void_to_s64)(void);
typedef const char *(*fn_type_void_to_const_charp)(void);
typedef s16 (*fn_type_s16_to_s16)(s16);
typedef void (*fn_type_voidp_to_void)(void*);
typedef s32 (*fn_type_voidp_to_s32)(void*);
typedef s64 (*fn_type_voidp_s64_to_s64)(void*, s64);
typedef s8  (*fn_type_s32_s8_to_s8)(s32, s8);
typedef void (*fn_type_s32p_to_void)(s32*);
typedef s8 (*fn_type_s32_to_s8)(s32);
typedef s8 (*fn_type_void_to_s8)();
typedef s32 (*fn_type_s32_to_s32)(s32);
typedef s64 (*fn_type_s32_to_s64)(s32);
typedef s32 (*fn_type_s32_s32_to_s32)(s32, s32);
typedef s64 (*fn_type_s64_to_s64)(s64);
typedef s64 (*fn_type_s64_s64_to_s64)(s64, s64);
typedef s64 (*fn_type_s64_s64_s64_to_s64)(s64, s64, s64);
typedef s64 (*fn_type_s64_s64_s64_s64_s64_to_s64)(s64, s64, s64, s64, s64);
typedef s64 (*fn_type_s64_s64_s64_s64_s64_s64_to_s64)(s64, s64, s64, s64, s64, s64);
typedef s32 (*fn_type__void_to_s32__to_s32)(fn_type_void_to_s32);

typedef u8 (*fn_type_void_to_u8)(void);

typedef struct {
  s64 x;
  s64 y;
} Test_128bit;

typedef Test_128bit (*fn_type_s64_to_test_128bit_struct)(s64);

bool
spec_check_mass_result(
  const Mass_Result *result
) {
  if (result->tag == Mass_Result_Tag_Success) return true;
  slice_print(result->Error.details.message);
  printf("\n  at ");
  source_range_print_start_position(&result->Error.details.source_range);
  return false;
}

static Compilation_Context test_context = {0};
static Slice test_file_name = slice_literal_fields("_test_.mass");
static Module test_module = {0};

void
test_init_module(
  Slice source
) {
  test_module = (Module) {
    .source_file = {test_file_name, source},
    .export_scope = test_context.scope,
  };
  test_context.module = &test_module;
}

static Value *
test_program_inline_source_base(
  const char *id,
  Compilation_Context *context,
  const char *source
) {
  test_init_module(slice_from_c_string(source));
  program_parse(context);
  return scope_lookup_force(context, context->scope, slice_from_c_string(id));
}

fn_type_opaque
test_program_inline_source_function(
  const char *function_id,
  Compilation_Context *context,
  const char *source
) {
  Value *value = test_program_inline_source_base(function_id, context, source);
  if (!spec_check_mass_result(context->result)) return 0;
  if (!value) return 0;
  Jit jit;
  jit_init(&jit, context->program);
  program_jit(&jit);
  if (!spec_check_mass_result(context->result)) return 0;
  return value_as_function(&jit, value);
}


spec("source") {

  before_each() {
    compilation_context_init(allocator_system, &test_context);
  }

  after_each() {
    compilation_context_deinit(&test_context);
  }

  describe("Scope") {
    it("should be able to set and lookup values") {
      Value *test = value_from_s64(test_context.allocator, 42);
      Scope *root_scope = scope_make(test_context.allocator, 0);
      scope_define(root_scope, slice_literal("test"), (Scope_Entry) {
        .type = Scope_Entry_Type_Value,
        .value = test,
      });
      Scope_Entry *entry = scope_lookup(root_scope, slice_literal("test"));
      check(entry->type == Scope_Entry_Type_Value);
      check(entry->value == test);
    }

    it("should be able to lookup things from parent scopes") {
      Value *global = value_from_s64(test_context.allocator, 42);
      Scope *root_scope = scope_make(test_context.allocator, 0);
      scope_define(root_scope, slice_literal("global"), (Scope_Entry) {
        .type = Scope_Entry_Type_Value,
        .value = global,
      });

      Value *level_1_test = value_from_s64(test_context.allocator, 1);
      Scope *scope_level_1 = scope_make(test_context.allocator, root_scope);
      scope_define(scope_level_1, slice_literal("test"), (Scope_Entry) {
        .type = Scope_Entry_Type_Value,
        .value = level_1_test,
      });

      Value *level_2_test = value_from_s64(test_context.allocator, 1);
      Scope *scope_level_2 = scope_make(test_context.allocator, scope_level_1);
      scope_define(scope_level_2, slice_literal("test"),  (Scope_Entry) {
        .type = Scope_Entry_Type_Value,
        .value = level_2_test,
      });

      Scope_Entry *entry =
        scope_lookup(scope_level_2, slice_literal("global"));
      check(entry->type == Scope_Entry_Type_Value);
      check(entry->value == global);
    }
  }

  describe("Tokenizer") {
    it("should be able to tokenize an empty string") {
      Slice source = slice_literal("");

      Array_Const_Token_Ptr tokens;
      Mass_Result result =
        tokenize(test_context.allocator, &(Source_File){test_file_name, source}, &tokens);
      check(result.tag == Mass_Result_Tag_Success);
      check(dyn_array_length(tokens) == 0);
    }

    it("should be able to tokenize a comment") {
      Slice source = slice_literal("// foo\n");
      Array_Const_Token_Ptr tokens;
      Mass_Result result =
        tokenize(test_context.allocator, &(Source_File){test_file_name, source}, &tokens);
      check(result.tag == Mass_Result_Tag_Success);
      check(dyn_array_length(tokens) == 0);
    }

    it("should be able to turn newlines into fake semicolon tokens on top level") {
      Slice source = slice_literal("foo\n");
      Array_Const_Token_Ptr tokens;
      Mass_Result result =
        tokenize(test_context.allocator, &(Source_File){test_file_name, source}, &tokens);
      check(result.tag == Mass_Result_Tag_Success);
      check(dyn_array_length(tokens) == 2);
      const Token *new_line = *dyn_array_get(tokens, 1);
      check(new_line->tag == Token_Tag_Operator);
      check(slice_equal(new_line->source, slice_literal(";")));
    }

    it("should be able to parse hex integers") {
      Slice source = slice_literal("0xCAFE");
      Array_Const_Token_Ptr tokens;
      Mass_Result result =
        tokenize(test_context.allocator, &(Source_File){test_file_name, source}, &tokens);
      check(result.tag == Mass_Result_Tag_Success);
      check(dyn_array_length(tokens) == 1);
      const Token *token = *dyn_array_get(tokens, 0);
      check(token->tag == Token_Tag_Value);
      check(slice_equal(token->source, slice_literal("0xCAFE")));
      check(token->Value.value->descriptor == &descriptor_u16);
      check(token->Value.value->operand.tag == Operand_Tag_Immediate);
      u64 bits = operand_immediate_value_up_to_u64(&token->Value.value->operand);
      check(bits == 0xCAFE, "Expected 0xCAFE, got 0x%" PRIx64, bits);
    }

    it("should be able to parse binary integers") {
      Slice source = slice_literal("0b100");
      Array_Const_Token_Ptr tokens;
      Mass_Result result =
        tokenize(test_context.allocator, &(Source_File){test_file_name, source}, &tokens);
      check(result.tag == Mass_Result_Tag_Success);
      check(dyn_array_length(tokens) == 1);
      const Token *token = *dyn_array_get(tokens, 0);
      check(token->tag == Token_Tag_Value);
      check(slice_equal(token->source, slice_literal("0b100")));
      check(token->Value.value->descriptor == &descriptor_u8);
      check(token->Value.value->operand.tag == Operand_Tag_Immediate);
      u64 bits = operand_immediate_value_up_to_u64(&token->Value.value->operand);
      check(bits == 0b100, "Expected 0x8, got 0x%" PRIx64, bits);
    }

    it("should be able to tokenize a sum of integers") {
      Slice source = slice_literal("12 + foo123");
      Array_Const_Token_Ptr tokens;
      Mass_Result result =
        tokenize(test_context.allocator, &(Source_File){test_file_name, source}, &tokens);
      check(result.tag == Mass_Result_Tag_Success);
      check(dyn_array_length(tokens) == 3);

      const Token *a_num = *dyn_array_get(tokens, 0);
      check(a_num->tag == Token_Tag_Value);
      check(slice_equal(a_num->source, slice_literal("12")));

      const Token *plus = *dyn_array_get(tokens, 1);
      check(plus->tag == Token_Tag_Operator);
      check(slice_equal(plus->source, slice_literal("+")));

      const Token *id = *dyn_array_get(tokens, 2);
      check(id->tag == Token_Tag_Id);
      check(slice_equal(id->source, slice_literal("foo123")));
    }

    it("should be able to tokenize groups") {
      Slice source = slice_literal("(x)");
      Array_Const_Token_Ptr tokens;
      Mass_Result result =
        tokenize(test_context.allocator, &(Source_File){test_file_name, source}, &tokens);
      check(result.tag == Mass_Result_Tag_Success);
      check(dyn_array_length(tokens) == 1);

      const Token *paren = *dyn_array_get(tokens, 0);
      check(paren->tag == Token_Tag_Group);
      check(paren->Group.tag == Token_Group_Tag_Paren);
      check(dyn_array_length(paren->Group.children) == 1);
      check(slice_equal(paren->source, slice_literal("(x)")));

      const Token *id = *dyn_array_get(paren->Group.children, 0);
      check(id->tag == Token_Tag_Id);
    }

    it("should be able to tokenize strings") {
      Slice source = slice_literal("\"foo 123\"");
      Array_Const_Token_Ptr tokens;
      Mass_Result result =
        tokenize(test_context.allocator, &(Source_File){test_file_name, source}, &tokens);
      check(result.tag == Mass_Result_Tag_Success);
      check(dyn_array_length(tokens) == 1);
      const Token *string = *dyn_array_get(tokens, 0);
      check(slice_equal(string->source, slice_literal("\"foo 123\"")));
    }

    it("should be able to tokenize nested groups with different braces") {
      Slice source = slice_literal("{[]}");
      Array_Const_Token_Ptr tokens;
      Mass_Result result =
        tokenize(test_context.allocator, &(Source_File){test_file_name, source}, &tokens);
      check(result.tag == Mass_Result_Tag_Success);
      check(dyn_array_length(tokens) == 1);

      const Token *curly = *dyn_array_get(tokens, 0);
      check(curly->tag == Token_Tag_Group);
      check(curly->Group.tag == Token_Group_Tag_Curly);
      check(dyn_array_length(curly->Group.children) == 1);
      check(slice_equal(curly->source, slice_literal("{[]}")));

      const Token *square = *dyn_array_get(curly->Group.children, 0);
      check(square->tag == Token_Tag_Group);
      check(square->Group.tag == Token_Group_Tag_Square);
      check(dyn_array_length(square->Group.children) == 0);
      check(slice_equal(square->source, slice_literal("[]")));
    }

    it("should be able to tokenize complex input") {
      Slice source = slice_literal(
        "foo :: (x: s8) -> {\n"
        "  return x + 3;\n"
        "}"
      );
      Array_Const_Token_Ptr tokens;
      Mass_Result result =
        tokenize(test_context.allocator, &(Source_File){test_file_name, source}, &tokens);
      check(result.tag == Mass_Result_Tag_Success);
    }

    it("should report a failure when encountering a brace that is not closed") {
      Slice source = slice_literal("(foo");
      Array_Const_Token_Ptr tokens;
      Mass_Result result =
        tokenize(test_context.allocator, &(Source_File){test_file_name, source}, &tokens);
      check(result.tag == Mass_Result_Tag_Error);
      Parse_Error *error = &result.Error.details;
      check(slice_equal(error->source_range.file->path, test_file_name));
      check(error->source_range.offsets.from == 4);
      check(error->source_range.offsets.to == 4);
      check(slice_equal(error->message, slice_literal("Unexpected end of file. Expected a closing brace.")));
    }

    it("should report a failure when encountering a mismatched brace that") {
      Slice source = slice_literal("(foo}");
      Array_Const_Token_Ptr tokens;
      Mass_Result result =
        tokenize(test_context.allocator, &(Source_File){test_file_name, source}, &tokens);
      check(result.tag == Mass_Result_Tag_Error);
      Parse_Error *error = &result.Error.details;
      check(slice_equal(error->source_range.file->path, test_file_name));
      check(error->source_range.offsets.from == 4);
      check(error->source_range.offsets.to == 4);
      check(slice_equal(error->message, slice_literal("Mismatched closing brace")));
    }
  }

  describe("Top Level Statements") {
    it("should be reported when encountering unknown top level statement") {
      test_program_inline_source_base("main", &test_context, "foo bar");
      check(test_context.result->tag == Mass_Result_Tag_Error);
      check(slice_equal(
        slice_literal("Could not parse a top level statement"),
        test_context.result->Error.details.message
      ));
    }
  }

  describe("Raw Machine Code") {
    #ifdef _WIN32
    // This test relies on Windows calling convention
    it("should be able to include raw machine code bytes") {
      fn_type_void_to_s64 checker = (fn_type_void_to_s64)test_program_inline_source_function(
        "foo", &test_context,
        "foo :: () -> (result : s64) {"
          "inline_machine_code_bytes(0x48, 0xC7, 0xC0, 0x2A, 0x00, 0x00, 0x00)"
          "result"
        "}"
      );
      check(checker);
      check(checker() == 42);
    }
    #endif

    #ifdef _WIN32
    // This test relies on Windows calling convention
    it("should be able to retrieve the register used for a particular value") {
      fn_type_void_to_s8 checker = (fn_type_void_to_s8)test_program_inline_source_function(
        "foo", &test_context,
        "foo :: (x : s64) -> (Register_64) {"
          "operand_variant_of(x)"
        "}"
      );
      check(checker);
      Register actual = checker();
      Register expected = Register_C;
      check(actual == expected, "Expected %d, got %d", expected, actual);
    }
    #endif

    it("should be able to reference a declared label in raw machine code bytes") {
      // TODO only run on X64 hosts
      fn_type_void_to_s64 checker = (fn_type_void_to_s64)test_program_inline_source_function(
        "foo", &test_context,
        "foo :: () -> (s64) {"
          "goto start;"
          "label from_machine_code;"
          "return 42;"
          "label start;"
          // "goto from_machine_code;"
          "inline_machine_code_bytes(0xE9, from_machine_code);"
          "10"
        "}"
      );
      check(checker);
      check(checker() == 42);
    }

  }

  #ifdef _WIN32
  describe("Win32: Structured Exceptions") {
    it("should be unwind stack on hardware exception on Windows") {
      Mass_Result result =
        program_import_file(&test_context, slice_literal("fixtures\\error_runtime_divide_by_zero"));
      check(result.tag == Mass_Result_Tag_Success);
      Value *main = scope_lookup_force(
        &test_context,
        test_context.scope,
        slice_literal("main")
      );
      check(main);

      Jit jit;
      jit_init(&jit, test_context.program);
      program_jit(&jit);
      check(spec_check_mass_result(test_context.result));

      fn_type_s32_to_s32 checker = (fn_type_s32_to_s32)value_as_function(&jit, main);

      volatile bool caught_exception = false;
      __try {
        checker(0);
      }
      __except(EXCEPTION_EXECUTE_HANDLER) {
        caught_exception = true;
      }
      check(caught_exception);
    }
  }
  #endif

  describe("Functions") {
    it("should be able to parse and run a void -> s64 function") {
      fn_type_void_to_s64 checker = (fn_type_void_to_s64)test_program_inline_source_function(
        "foo", &test_context,
        "foo :: () -> (s64) { 42 }"
      );
      check(checker);
      check(checker() == 42);
    }

    it("should be able to parse and run a function with 5 arguments") {
      fn_type_void_to_s8 checker = (fn_type_void_to_s8)test_program_inline_source_function(
        "foo", &test_context,
        "foo :: (x1: s8, x2 : s8, x3 : s8, x4 : s8, x5 : s8) -> (s8) { x5 }"
      );
      check(checker);
      check(checker(1, 2, 3, 4, 5) == 5);
    }

    it("should correctly save volatile registers when calling other functions") {
      fn_type_s64_to_s64 checker = (fn_type_s64_to_s64)test_program_inline_source_function(
        "outer", &test_context,
        "inner :: (x : s64) -> () { x = 21 };"
        "outer :: (x : s64) -> (s64) { inner(1); x }"
      );
      check(checker);
      s64 actual = checker(42);
      check(actual == 42);
    }

    it("should be able to parse and run a s64 -> s64 function") {
      fn_type_s64_to_s64 checker = (fn_type_s64_to_s64)test_program_inline_source_function(
        "foo", &test_context,
        "foo :: (x : s64) -> (s64) { x }"
      );
      check(checker);
      check(checker(42) == 42);
    }

    it("should be able to define, assign and lookup an s64 variable on the stack") {
      fn_type_void_to_s64 checker = (fn_type_void_to_s64)test_program_inline_source_function(
        "foo", &test_context,
        "foo :: () -> (s64) { y : s8; y = 10; x := 21; x = 32; x + y }"
      );
      check(checker);
      check(checker() == 42);
    }

    it("should be able to parse and run multiple function definitions") {
      fn_type_void_to_s32 checker = (fn_type_void_to_s32)test_program_inline_source_function(
        "proxy", &test_context,
        "proxy :: () -> (s32) { plus(1, 2); plus(30 + 10, 2) }\n"
        "plus :: (x : s32, y : s32) -> (s32) { x + y }"
      );
      check(checker);
      s32 answer = checker();
      check(answer == 42);
    }

    it("should be able to define a local function") {
      fn_type_void_to_s64 checker = (fn_type_void_to_s64)test_program_inline_source_function(
        "checker", &test_context,
        "checker :: () -> (s64) { local :: () -> (s64) { 42 }; local() }"
      );
      check(checker);
      s64 answer = checker();
      check(answer == 42);
    }

    it("should be able to parse and run functions with local overloads") {
      fn_type_s32_to_s64 checker = (fn_type_s32_to_s64)test_program_inline_source_function(
        "checker", &test_context,
        "size_of :: (x : s32) -> (s64) { 4 }\n"
        "checker :: (x : s32) -> (s64) { size_of :: (x : s64) -> (s64) { 8 }; size_of(x) }"
      );
      check(checker);
      s64 size = checker(0);
      check(size == 4);
    }

    it("should be able to parse and run functions with overloads") {
      Slice source = slice_literal(
        "size_of :: (x : s32) -> (s64) { 4 }\n"
        "size_of :: (x : s64) -> (s64) { 8 }\n"
        "checker_s64 :: (x : s64) -> (s64) { size_of(x) }\n"
        "checker_s32 :: (x : s32) -> (s64) { size_of(x) }\n"
      );

      test_init_module(source);
      MASS_ON_ERROR(program_parse(&test_context)) {
        check(false, "Failed parsing");
      }

      Value *checker_s64 =
        scope_lookup_force(&test_context, test_context.scope, slice_literal("checker_s64"));
      Value *checker_32 =
        scope_lookup_force(&test_context, test_context.scope, slice_literal("checker_s32"));

      Jit jit;
      jit_init(&jit, test_context.program);
      program_jit(&jit);

      {
        s64 size = ((fn_type_s64_to_s64)value_as_function(&jit, checker_s64))(0);
        check(size == 8);
      }

      {
        s64 size = ((fn_type_s32_to_s64)value_as_function(&jit, checker_32))(0);
        check(size == 4);
      }
    }

    it("should report an overload overlap") {
      test_program_inline_source_base(
        "test", &test_context,
        "overload :: (x : s64, y : s32) -> () { }\n"
        "overload :: (x : s32, y : s64) -> () { }\n"
        "test :: () -> () { overload(1, 2) }"
      );
      check(test_context.result->tag == Mass_Result_Tag_Error);
      Parse_Error *error = &test_context.result->Error.details;
      check(slice_equal(slice_literal("Could not decide which overload to pick"), error->message));
    }

    it("should be able to have an explicit return") {
      fn_type_s32_to_s32 checker = (fn_type_s32_to_s32)test_program_inline_source_function(
        "checker", &test_context,
        "checker :: (x : s32) -> (s32) { return x }"
      );
      check(checker);
      s32 actual = checker(42);
      check(actual == 42);
    }

    it("should be able to assign a fn to a variable and call through pointer") {
      fn_type_void_to_s32 checker = (fn_type_void_to_s32)test_program_inline_source_function(
        "checker", &test_context,
        "checker :: () -> (s32) { local := () -> (s32) { 42 }; local() }"
      );
      check(checker);
      s32 actual = checker();
      check(actual == 42);
    }

    it("should be able to parse typed definition and assignment in the same statement") {
      fn_type_void_to_s32 checker = (fn_type_void_to_s32)test_program_inline_source_function(
        "test_fn", &test_context,
        "test_fn :: () -> (s32) {"
          "result : s32 = 42;"
          "result"
        "}"
      );
      check(checker);
      check(checker() == 42);
    }

    it("should be able to run fibonnacii") {
      fn_type_s64_to_s64 fibonnacci = (fn_type_s64_to_s64)test_program_inline_source_function(
        "fibonnacci", &test_context,
        "fibonnacci :: (n : s64) -> (s64) {"
          "if (n < 2) { return n };"
          "fibonnacci(n - 1) + fibonnacci(n - 2)"
        "}"
      );
      check(fibonnacci);

      check(fibonnacci(0) == 0);
      check(fibonnacci(1) == 1);
      check(fibonnacci(10) == 55);
    }

    it("should report an error when encountering invalid pointer type") {
      test_program_inline_source_base(
        "main", &test_context,
        "main :: (arg : [s32 s32]) -> () {}"
      );
      check(test_context.result->tag == Mass_Result_Tag_Error);
      Parse_Error *error = &test_context.result->Error.details;
      check(slice_equal(slice_literal("Pointer type must have a single type inside"), error->message));
    }

    it("should report an error when encountering multiple return types") {
      test_program_inline_source_base(
        "exit", &test_context,
        "exit :: (status: s32) -> (s32, s32) {}"
      );
      check(test_context.result->tag == Mass_Result_Tag_Error);
      Parse_Error *error = &test_context.result->Error.details;
      check(slice_equal(slice_literal("Multiple return types are not supported at the moment"), error->message));
    }

    it("should report an error when encountering wrong argument type to external()") {
      test_program_inline_source_base(
        "exit", &test_context,
        "exit :: (status: s32) -> () external(\"kernel32.dll\", 42)"
      );
      check(test_context.result->tag == Mass_Result_Tag_Error);
      Parse_Error *error = &test_context.result->Error.details;
      check(slice_equal(slice_literal("Second argument to external() must be a literal string"), error->message));
    }

    it("should report an error when non-type id is being used as a type") {
      test_program_inline_source_base(
        "main", &test_context,
        "foo :: () -> () {};"
        "main :: (arg : foo) -> () {}"
      );
      check(test_context.result->tag == Mass_Result_Tag_Error);
      Parse_Error *error = &test_context.result->Error.details;
      check(slice_equal(slice_literal("foo is not a type"), error->message));
    }

    it("should report an error when non-type token is being used as a type") {
      test_program_inline_source_base(
        "main", &test_context,
        "main :: (arg : 42) -> () {}"
      );
      check(test_context.result->tag == Mass_Result_Tag_Error);
      Parse_Error *error = &test_context.result->Error.details;
      check(slice_equal(slice_literal("42 is not a type"), error->message));
    }

    it("should report an error when encountering an unknown type") {
      Mass_Result result =
        program_import_file(&test_context, slice_literal("fixtures\\error_unknown_type"));
      check(result.tag == Mass_Result_Tag_Success);
      scope_lookup_force(&test_context, test_context.scope, slice_literal("main"));
      check(test_context.result->tag == Mass_Result_Tag_Error);
      Parse_Error *error = &test_context.result->Error.details;
      check(slice_equal(slice_literal("Could not find type s33"), error->message));
    }
  }

  describe("Operators") {
    it("should be able to parse and run a triple plus function") {
      fn_type_s64_s64_s64_to_s64 checker = (fn_type_s64_s64_s64_to_s64)test_program_inline_source_function(
        "plus", &test_context,
        "plus :: (x : s64, y : s64, z : s64) -> (s64) { x + y + z }"
      );
      check(checker);
      check(checker(30, 10, 2) == 42);
    }

    it("should be able to parse and run a subtraction of a negative number") {
      fn_type_s64_to_s64 checker = (fn_type_s64_to_s64)test_program_inline_source_function(
        "plus_one", &test_context,
        "plus_one :: (x : s64) -> (s64) { x - -1 }"
      );
      check(checker);
      check(checker(41) == 42);
    }

    it("should be able to parse and run a sum passed to another function as an argument") {
      fn_type_void_to_s64 checker = (fn_type_void_to_s64)test_program_inline_source_function(
        "plus", &test_context,
        "id :: (ignored : s64, x : s64) -> (s64) { x }\n"
        "plus :: () -> (s64) { x : s64 = 40; y : s64 = 2; id(0, x + y) }"
      );
      check(checker);
      check(checker() == 42);
    }

    it("should be possible to define infix operators") {
      fn_type_void_to_s64 checker = (fn_type_void_to_s64)test_program_inline_source_function(
        "test", &test_context,
        "operator 18 (++ x) { x = x + 1; x };"
        "test :: () -> (s64) { y := 41; ++y }"
      );
      check(checker);
      check(checker() == 42);
    }

    it("should be possible to define postfix operators") {
      fn_type_void_to_s64 checker = (fn_type_void_to_s64)test_program_inline_source_function(
        "test", &test_context,
        "operator 18 (x ++) { result := x; x = x + 1; result };"
        "test :: () -> (s64) { y := 42; y++ }"
      );
      check(checker);
      check(checker() == 42);
    }

    it("should be possible to define an overloaded postfix and infix operator") {
      fn_type_void_to_s64 checker = (fn_type_void_to_s64)test_program_inline_source_function(
        "test", &test_context,
        "operator 18 (++ x) { x = x + 1; x };"
        "operator 18 (x ++) { result := x; x = x + 1; result };"
        "test :: () -> (s64) { y := 41; ++y++ }"
      );
      check(checker);
      check(checker() == 42);
    }

    it("should be possible to define infix operators") {
      fn_type_void_to_s64 checker = (fn_type_void_to_s64)test_program_inline_source_function(
        "test", &test_context,
        "operator 15 (x ** y) { x * y };"
        "test :: () -> (s64) { 21 ** 2 }"
      );
      check(checker);
      check(checker() == 42);
    }

    it("should report an error when defining an overloaded infix operator") {
      test_program_inline_source_base(
        "dummy", &test_context,
        "operator 15 (x ** y) { x * y };"
        "operator 15 (x ** y) { x * y };"
        "dummy :: () -> (s64) { 21 ** 2 }"
      );
      check(test_context.result->tag == Mass_Result_Tag_Error);
      Parse_Error *error = &test_context.result->Error.details;
      check(slice_equal(
        slice_literal("There is already an infix operator **. You can only have one definition for prefix and one for infix or suffix."),
        error->message
      ));
    }

    it("should report an error when defining an overloaded infix and postfix operator") {
      test_program_inline_source_base(
        "dummy", &test_context,
        "operator 15 (x ** y) { x * y };"
        "operator 15 (x **) { x * x };"
        "dummy :: () -> (s64) { 21 ** 2 }"
      );
      check(test_context.result->tag == Mass_Result_Tag_Error);
      Parse_Error *error = &test_context.result->Error.details;
      check(slice_equal(
        slice_literal("There is already an infix operator **. You can only have one definition for prefix and one for infix or suffix."),
        error->message
      ));
    }
  }

  describe("Compile Time Execution") {
    it("should be able to call a function at compile time") {
      Value *status = test_program_inline_source_base(
        "STATUS_CODE", &test_context,
        "STATUS_CODE :: the_answer();"
        "the_answer :: () -> (s8) { 42 }"
      );

      check(status);
      check(descriptor_is_integer(status->descriptor));
      check(status->operand.tag == Operand_Tag_Immediate);
      check(status->operand.byte_size == 1);
      check(operand_immediate_memory_as_s8(&status->operand) == 42);
    }

    it("should be able to to do nested compile time calls") {
      Value *result = test_program_inline_source_base(
        "RESULT", &test_context,
        "RESULT :: get_a();"
        "B :: get_b();"
        "get_a :: () -> (s8) { B };"
        "get_b :: () -> (s8) { 42 }"
      );

      check(result);
      check(descriptor_is_integer(result->descriptor));
      check(result->operand.tag == Operand_Tag_Immediate);
      check(result->operand.byte_size == 1);
      check(operand_immediate_memory_as_s8(&result->operand) == 42);
    }

    xit("should not be able to use runtime values in a static context") {
      test_program_inline_source_base(
        "test", &test_context,
        "test :: () -> (s64) { foo := 42; @( foo ) }"
      );
      check(test_context.result->tag == Mass_Result_Tag_Error);
      Parse_Error *error = &test_context.result->Error.details;
      check(slice_equal(
        slice_literal("Undefined variable foo"),
        error->message
      ));
    }
  }

  describe("Macro") {
    it("should be able to parse and run macro id function") {
      fn_type_void_to_s64 checker = (fn_type_void_to_s64)test_program_inline_source_function(
        "test", &test_context,
        "id :: macro (x : s64) -> (s64) { x }\n"
        "test :: () -> (s64) { id(42) }"
      );
      check(checker);
      check(checker() == 42);
    }

    it("should be able to parse and run macro id fn with an explicit return and an immediate arg") {
      fn_type_void_to_s64 checker = (fn_type_void_to_s64)test_program_inline_source_function(
        "test", &test_context,
        "id :: macro (x : s64) -> (s64) { if (x > 0) { return 20 }; x }\n"
        "test :: () -> (s64) { id(42) + 1 }"
      );
      check(checker);
      check(checker() == 21);
    }

    it("should be able to parse and run macro with a literal argument") {
      fn_type_void_to_s8 checker = (fn_type_void_to_s8)test_program_inline_source_function(
        "test", &test_context,
        "broken_plus :: macro (x : u8, 0) -> (u8) { x + 1 }\n"
        "broken_plus :: macro (x : u8, y : u8) -> (u8) { x + y }\n"
        "test :: () -> (u8) { broken_plus(41, 0) }"
      );
      check(checker);
      check(checker() == 42);
    }

    it("should allow changes to the passed arguments to macro function") {
      fn_type_void_to_s64 checker = (fn_type_void_to_s64)test_program_inline_source_function(
        "test", &test_context,
        "process :: macro (y : s64) -> () { y = 42; }\n"
        "test :: () -> (s64) { x := 20; process(x); x }"
      );
      check(checker);
      check(checker() == 42);
    }

    xit("should not allow changes to the passed arguments to inline function") {
      fn_type_void_to_s64 checker = (fn_type_void_to_s64)test_program_inline_source_function(
        "test", &test_context,
        "process :: inline (y : s64) -> () { y = 20; }\n"
        "test :: () -> (s64) { x := 42; process(x); x }"
      );
      check(checker);
      check(checker() == 42);
    }

    it("should be able to define and use a syntax macro without a capture") {
      fn_type_void_to_s32 checker = (fn_type_void_to_s32)test_program_inline_source_function(
        "checker", &test_context,
        "syntax (\"the\" \"answer\") 42;"
        "checker :: () -> (s32) { the answer }"
      );
      check(checker);
      check(checker() == 42);
    }

    it("should be able to define and use a syntax macro matching start and end of statement") {
      fn_type_void_to_s64 checker = (fn_type_void_to_s64)test_program_inline_source_function(
        "checker", &test_context,
        "syntax (^ \"foo\" $) 42;"
        "checker :: () -> (s64) { foo := 20; foo }"
      );
      check(checker);
      check(checker() == 42);
    }

    it("should be able to define and use a syntax macro matching a curly brace block") {
      fn_type_void_to_s64 checker = (fn_type_void_to_s64)test_program_inline_source_function(
        "checker", &test_context,
        "syntax (^ \"block\" {}@body $) body();"
        "checker :: () -> (s64) { block { 42 } }"
      );
      check(checker);
      check(checker() == 42);
    }

    it("should be able to define and use a syntax macro matching a sequence at the end") {
      fn_type_void_to_s64 checker = (fn_type_void_to_s64)test_program_inline_source_function(
        "checker", &test_context,
        "syntax (^ \"comment\" ..@ignore $);"
        "checker :: () -> (s64) { x := 42; comment x = x + 1; x }"
      );
      check(checker);
      check(checker() == 42);
    }

    it("should be able to define and use a syntax macro with a capture") {
      fn_type_void_to_s32 checker = (fn_type_void_to_s32)test_program_inline_source_function(
        "checker", &test_context,
        // TODO figure out what should be the rule for subtracting unsigned integers
        "syntax (\"negative\" .@x) (cast(s32, 0) - x());"
        "checker :: () -> (s32) { negative 42 }"
      );
      check(checker);
      check(checker() == -42);
    }

    it("should be able to define and use a macro for while loop") {
      program_import_file(&test_context, slice_literal("lib\\prelude"));
      fn_type_s32_to_s32 sum_up_to = (fn_type_s32_to_s32)test_program_inline_source_function(
        "sum_up_to", &test_context,
        "sum_up_to :: (x : s32) -> (s32) {"
          "sum : s32;"
          "sum = 0;"
          "while (x >= 0) {"
            "sum = sum + x;"
            "x = x + (-1);"
          "};"
          "return sum"
        "}"
      );
      check(sum_up_to);
      check(sum_up_to(0) == 0);
      check(sum_up_to(1) == 1);
      check(sum_up_to(2) == 3);
      check(sum_up_to(3) == 6);
    }

    it("should report an error for macro external functions") {
      test_program_inline_source_base(
        "test", &test_context,
        "ExitProcess :: macro (x : s64) -> (s64) external(\"kernel32.dll\", \"ExitProcess\")\n"
        "test :: () -> (s64) { ExitProcess(42) }"
      );
      check(test_context.result->tag == Mass_Result_Tag_Error);
    }

    it("should report an end of statement marker not at the start of a syntax definition") {
      test_program_inline_source_base(
        "dummy", &test_context,
        "syntax (\"foo\" ^ .@_ );"
        "dummy :: () -> () {}"
      );
      check(test_context.result->tag == Mass_Result_Tag_Error);
      Parse_Error *error = &test_context.result->Error.details;
      check(slice_equal(
        slice_literal("^ operator (statement start match) can only appear at the start of the pattern."),
        error->message
      ));
    }

    it("should report an end of statement marker not at the end of a syntax definition") {
      test_program_inline_source_base(
        "dummy", &test_context,
        "syntax (\"foo\" $ .@_ );"
        "dummy :: () -> () {}"
      );
      check(test_context.result->tag == Mass_Result_Tag_Error);
      Parse_Error *error = &test_context.result->Error.details;
      check(slice_equal(
        slice_literal("$ operator (statement end match) can only appear at the end of the pattern."),
        error->message
      ));
    }

    it("should be able to parse and run macro id function") {
      fn_type_void_to_s64 checker = (fn_type_void_to_s64)test_program_inline_source_function(
        "test", &test_context,
        "FOO :: 42\n"
        "id :: macro (x : s64) -> (s64) { x }\n"
        "BAR :: id(FOO)\n"
        "test :: () -> (s64) { BAR }"
      );
      check(checker);
      check(checker() == 42);
    }
  }

  describe("if / else") {
    it("should be able to parse and run if statement") {
      fn_type_s32_to_s8 checker = (fn_type_s32_to_s8)test_program_inline_source_function(
        "is_positive", &test_context,
        "is_positive :: (x : s32) -> (s8) {"
          "if (x < 0) { return 0 };"
          "1"
        "}"
      );
      check(checker);
      check(checker(42) == 1);
      check(checker(-2) == 0);
    }

    it("should report an error for an `if` statement without a body or a condition") {
      test_program_inline_source_base(
        "main", &test_context,
        "main :: () -> () { if; }"
      );
      check(test_context.result->tag == Mass_Result_Tag_Error);
      Parse_Error *error = &test_context.result->Error.details;
      check(slice_equal(slice_literal("`if` keyword must be followed by an expression"), error->message));
    }

    it("should report an error for an `if` statement with an incorrect condition") {
      test_program_inline_source_base(
        "main", &test_context,
        "main :: () -> () { if (1 < 0) { 0 } 42; }"
      );
      check(test_context.result->tag == Mass_Result_Tag_Error);
      Parse_Error *error = &test_context.result->Error.details;
      check(slice_equal(slice_literal("Could not parse the expression"), error->message));
    }
  }

  describe("labels / goto") {
    it("should be able to parse and run a program with labels and goto") {
      fn_type_s32_to_s32 sum_up_to = (fn_type_s32_to_s32)test_program_inline_source_function(
        "sum_up_to", &test_context,
        "sum_up_to :: (x : s32) -> (s32) {"
          "sum : s32;"
          "sum = 0;"
          "label loop;"
          "if (x < 0) { return sum };"
          "sum = sum + x;"
          "x = x - 1;"
          "goto loop;"
          // FIXME This return is never reached and ideally should not be required
          //       but currently there is no way to track dead branches
          "sum"
        "}"
      );
      check(sum_up_to);
      check(sum_up_to(0) == 0);
      check(sum_up_to(1) == 1);
      check(sum_up_to(2) == 3);
      check(sum_up_to(3) == 6);
    }

    it("should be able to goto a label defined after the goto") {
      fn_type_void_to_s32 checker = (fn_type_void_to_s32)test_program_inline_source_function(
        "test", &test_context,
        "test :: () -> (s32) {"
          "x : s32 = 42;"
          "goto skip;"
          "x = 0;"
          "label skip;"
          "x"
        "}"
      );
      check(checker);
      check(checker() == 42);
    }

    it("should report an error when encountering wrong type of label identifier") {
      test_program_inline_source_base(
        "main", &test_context,
        "main :: (status: s32) -> () { x : s32; goto x; }"
      );
      check(test_context.result->tag == Mass_Result_Tag_Error);
      Parse_Error *error = &test_context.result->Error.details;
      check(slice_equal(slice_literal("x is not a label"), error->message));
    }
  }

  describe("Strings") {
    it("should parse and return C-compatible strings") {
      fn_type_void_to_const_charp checker = (fn_type_void_to_const_charp)test_program_inline_source_function(
        "checker", &test_context,
        "checker :: () -> ([s8]) { &\"test\" }"
      );
      check(checker);
      const char *string = checker();
      check(strcmp(string, "test") == 0);
    }
  }

  describe("Fixed Size Arrays") {
    it("should report an error when fixed size array size does not resolve to an integer") {
      test_program_inline_source_base(
        "test", &test_context,
        "BAR :: \"foo\"; "
        "test :: () -> (s8) {"
          "foo : s8[BAR];"
          "foo[0] = 42;"
          "foo[0]"
        "}"
      );
      check(test_context.result->tag == Mass_Result_Tag_Error);
      Parse_Error *error = &test_context.result->Error.details;
      check(slice_equal(slice_literal("Fixed size array size must be an unsigned integer"), error->message));
    }

    it("should be able to define a variable with a fixed size array type") {
      fn_type_void_to_s8 checker = (fn_type_void_to_s8)test_program_inline_source_function(
        "test", &test_context,
        "test :: () -> (s8) {"
          "foo : s8[64];"
          "foo[0] = 42;"
          "foo[0]"
        "}"
      );
      check(checker);
      u8 actual = checker();
      check(actual == 42);
    }
  }

  describe("User-defined Types") {
    it("should be able to parse fixed-bit sized type definitions") {
      fn_type_void_to_void checker = (fn_type_void_to_void)test_program_inline_source_function(
        "test", &test_context,
        "int8 :: bit_type(8);"
        "test :: () -> () {"
          "x : int8;"
        "}"
      );
      check(checker);
      checker();
    }

    it("should be able to parse struct definitions") {
      fn_type_void_to_s32 checker = (fn_type_void_to_s32)test_program_inline_source_function(
        "test", &test_context,
        "Point :: c_struct({ x : s32; y : s32; });"
        "test :: () -> (s32) {"
          "p : Point; p.x = 20; p.y = 22;"
          "p.x + p.y"
        "}"
      );
      check(checker);
      check(checker() == 42);
    }

    it("should report an error when field name is not an identifier") {
      test_program_inline_source_base(
        "main", &test_context,
        "Point :: c_struct({ x : s32; y : s32; });"
        "main :: () -> () { p : Point; p.(x) }"
      );
      check(test_context.result->tag == Mass_Result_Tag_Error);
      Parse_Error *error = &test_context.result->Error.details;
      check(slice_equal(slice_literal(
        "Right hand side of the . operator must be an identifier"), error->message
      ));
    }

    it("should be able to return structs while accepting other arguments") {
      fn_type_s64_to_test_128bit_struct checker = (fn_type_s64_to_test_128bit_struct)test_program_inline_source_function(
        "return_struct", &test_context,
        "Test_128bit :: c_struct({ x : s64; y : s64 });"
        "return_struct :: (x : s64) -> (Test_128bit) {"
          "result : Test_128bit;"
          "result.x = x;"
          "result.y = x / 2;"
          "result"
        "}"
      );
      check(checker);

      Test_128bit test_128bit = checker(42);
      check(test_128bit.x == 42);
      check(test_128bit.y == 21);
    }
  }

  describe("Unsigned Integers") {
    it("should be able to return unsigned integer literals") {
      fn_type_void_to_u8 checker = (fn_type_void_to_u8)test_program_inline_source_function(
        "return_200", &test_context,
        "return_200 :: () -> (u8) { 200 }"
      );
      check(checker);
      check(checker() == 200);
    }

    it("should use correct EFLAGS values when dealing with unsigned integers") {
      fn_type_void_to_u8 checker = (fn_type_void_to_u8)test_program_inline_source_function(
        "test", &test_context,
        "test :: () -> (s8) { x : u8 = 200; x < 0 }"
      );
      check(checker);
      check(checker() == false);
    }
  }

  describe("Signed Integers") {
    it("should parse and correctly deal with 16 bit values") {
      fn_type_s16_to_s16 checker = (fn_type_s16_to_s16)test_program_inline_source_function(
        "add_one", &test_context,
        "add_one :: (x : s16) -> (s16) { x + 1 }"
      );
      check(checker);
      check(checker(8) == 9);
    }
  }

  describe("PE32 Executables") {
    it("should parse and write out an executable that exits with status code 42") {
      Slice source = slice_literal(
        "main :: () -> () { ExitProcess(42) }\n"
        "ExitProcess :: (status : s32) -> (s64) external(\"kernel32.dll\", \"ExitProcess\")"
      );

      test_init_module(source);
      MASS_ON_ERROR(program_parse(&test_context)) {
        check(false, "Failed parsing");
      }

      Program *test_program = test_context.program;
      test_program->entry_point =
        scope_lookup_force(&test_context, test_context.scope, slice_literal("main"));
      check(test_program->entry_point->descriptor->tag != Descriptor_Tag_Any);
      check(spec_check_mass_result(test_context.result));

      write_executable("build\\test_parsed.exe", &test_context, Executable_Type_Cli);
    }

    it("should parse and write an executable that prints Hello, world!") {
      program_import_file(&test_context, slice_literal("fixtures\\hello_world"));
      Program *test_program = test_context.program;
      test_program->entry_point =
        scope_lookup_force(&test_context, test_context.scope, slice_literal("main"));
      check(test_program->entry_point);
      check(test_program->entry_point->descriptor->tag != Descriptor_Tag_Any);
      check(spec_check_mass_result(test_context.result));

      write_executable("build\\hello_world.exe", &test_context, Executable_Type_Cli);
    }
  }

  describe("Complex Examples") {
    it("should be able to run fizz buzz") {
      Mass_Result result = program_import_file(&test_context, slice_literal("lib\\prelude"));
      check(result.tag == Mass_Result_Tag_Success);
      result = program_import_file(&test_context, slice_literal("fixtures\\fizz_buzz"));
      check(result.tag == Mass_Result_Tag_Success);
      Program *test_program = test_context.program;

      Value *fizz_buzz = scope_lookup_force(
        &test_context, test_context.scope, slice_literal("fizz_buzz")
      );
      check(fizz_buzz);

      Jit jit;
      jit_init(&jit, test_program);
      program_jit(&jit);
      check(spec_check_mass_result(test_context.result));

      fn_type_void_to_void checker =
        (fn_type_void_to_void)value_as_function(&jit, fizz_buzz);
      checker();
    }
  }
}
