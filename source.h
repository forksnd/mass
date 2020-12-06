#ifndef SOURCE_H
#define SOURCE_H
#include "prelude.h"
#include "value.h"

typedef enum {
  Token_Type_Id = 1,
  Token_Type_Newline,
  Token_Type_Integer,
  Token_Type_Hex_Integer,
  Token_Type_Operator,
  Token_Type_String,
  Token_Type_Paren,
  Token_Type_Square,
  Token_Type_Curly,
  Token_Type_Value,
} Token_Type;

typedef struct Token {
  Token_Type type;
  Source_Range source_range;
  Slice source;
  union {
    Array_Token_Ptr children;
    Value *value;
    Slice string;
  };
} Token;
typedef dyn_array_type(Token) Array_Token;

typedef struct {
  Token_Type type;
  Slice source;
} Token_Pattern;
typedef dyn_array_type(Token_Pattern) Array_Token_Pattern;

typedef enum {
  Tokenizer_State_Default,
  Tokenizer_State_Integer,
  Tokenizer_State_Hex_Integer,
  Tokenizer_State_Operator,
  Tokenizer_State_Id,
  Tokenizer_State_String,
  Tokenizer_State_String_Escape,
  Tokenizer_State_Single_Line_Comment,
} Tokenizer_State;

typedef struct {
  const Token **tokens;
  u64 length;
} Token_View;

typedef dyn_array_type(Token_View) Array_Token_View;

typedef enum {
  Scope_Entry_Type_Value = 1,
  Scope_Entry_Type_Lazy_Constant_Expression,
} Scope_Entry_Type;

typedef struct {
  Token_View tokens;
  Scope *scope;
} Scope_Lazy_Constant_Expression;

typedef struct {
  Scope_Entry_Type type;
  union {
    Value *value;
    Scope_Lazy_Constant_Expression lazy_constant_expression;
  };
} Scope_Entry;
typedef dyn_array_type(Scope_Entry) Array_Scope_Entry;

hash_map_slice_template(Scope_Map, Array_Scope_Entry)
hash_map_slice_template(Macro_Replacement_Map, const Token *)

typedef struct {
  Array_Token_Pattern pattern;
  Array_Token_Ptr replacement;
  Array_Slice pattern_names;
} Macro;
typedef dyn_array_type(Macro *) Array_Macro_Ptr;

typedef bool (*Token_Statement_Matcher_Proc)
(Compilation_Context *context, Token_View , Scope *, Function_Builder *);
typedef dyn_array_type(Token_Statement_Matcher_Proc) Array_Token_Statement_Matcher_Proc;

typedef struct Scope {
  struct Scope *parent;
  Scope_Map *map;
  Array_Macro_Ptr macros;
  Array_Token_Statement_Matcher_Proc statement_matchers;
} Scope;

Scope *
scope_make(
  Allocator *allocator,
  Scope *parent
);

void
scope_define_value(
  Scope *scope,
  Slice name,
  Value *value
);

bool
token_parse_block(
  Compilation_Context *program,
  const Token *block,
  Scope *scope,
  Function_Builder *builder,
  Value *result_value
);

bool
token_rewrite_statement_if(
  Compilation_Context *program,
  Token_View state,
  Scope *scope,
  Function_Builder *builder
);
bool
token_rewrite_inline_machine_code_bytes(
  Compilation_Context *program,
  Token_View state,
  Scope *scope,
  Function_Builder *builder
);
bool
token_rewrite_assignment(
  Compilation_Context *program,
  Token_View state,
  Scope *scope,
  Function_Builder *builder
);
bool
token_rewrite_definition_and_assignment_statements(
  Compilation_Context *program,
  Token_View state,
  Scope *scope,
  Function_Builder *builder
);
bool
token_rewrite_definitions(
  Compilation_Context *program,
  Token_View state,
  Scope *scope,
  Function_Builder *builder
);
bool
token_rewrite_explicit_return(
  Compilation_Context *program,
  Token_View state,
  Scope *scope,
  Function_Builder *builder
);
bool
token_rewrite_goto(
  Compilation_Context *program,
  Token_View state,
  Scope *scope,
  Function_Builder *builder
);
bool
token_rewrite_constant_definitions(
  Compilation_Context *program,
  Token_View state,
  Scope *scope,
  Function_Builder *builder
);

void
program_push_error_from_bucket_buffer(
  Compilation_Context *context,
  Source_Range source_range,
  Bucket_Buffer *buffer
);

void
program_push_error_from_slice(
  Program *program,
  Source_Range source_range,
  Slice message
);

#define program_error_builder(_program_, _location_)\
  for(\
    struct {\
      Bucket_Buffer *buffer;\
      u8 number_print_buffer[32];\
    } _error_builder = {\
      .buffer = bucket_buffer_make(.allocator = allocator_system),\
    };\
    _error_builder.buffer;\
    program_push_error_from_bucket_buffer((_program_), (_location_), _error_builder.buffer),\
    _error_builder.buffer = 0\
  )
#define program_error_append_number(_format_, _number_)\
  do {\
    snprintf(\
      _error_builder.number_print_buffer,\
      countof(_error_builder.number_print_buffer),\
      _format_,\
      (_number_)\
    );\
    Slice _number_slice = slice_from_c_string(_error_builder.number_print_buffer);\
    bucket_buffer_append_slice(_error_builder.buffer, _number_slice);\
  } while (0)

#define program_error_append_slice(_slice_)\
  bucket_buffer_append_slice(_error_builder.buffer, (_slice_))

#define program_error_append_literal(_message_)\
  program_error_append_slice(slice_literal(_message_))

#endif SOURCE_H
