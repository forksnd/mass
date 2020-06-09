#include "bdd-for-c.h"
#include "windows.h"
#include <stdio.h>

#include "prelude.c"
#include "value.c"
#include "instruction.c"
#include "encoding.c"
#include "function.c"

Value *
fn_reflect(
  Function_Builder *builder,
  Descriptor *descriptor
) {
  Value_Overload *result = reserve_stack(builder, &descriptor_struct_reflection);
  // FIXME support all types
  assert(descriptor->type == Descriptor_Type_Struct);
  // FIXME support generic allocation of structs on the stack
  move_value(builder, result,
    maybe_get_if_single_overload(value_from_s32(descriptor->struct_.field_count)));
  return single_overload_value(result);
}

typedef struct Struct_Builder_Field {
  Descriptor_Struct_Field struct_field;
  struct Struct_Builder_Field *next;
} Struct_Builder_Field;

typedef struct {
  u32 offset;
  u32 field_count;
  Struct_Builder_Field *field_list;
} Struct_Builder;

Struct_Builder
struct_begin() {
  return (const Struct_Builder) {0};
}

Descriptor_Struct_Field *
struct_add_field(
  Struct_Builder *builder,
  Descriptor *descriptor,
  const char *name
) {
  Struct_Builder_Field *builder_field = temp_allocate(Struct_Builder_Field);

  u32 size = descriptor_byte_size(descriptor);
  builder->offset = align(builder->offset, size);

  builder_field->struct_field.name = name;
  builder_field->struct_field.descriptor = descriptor;
  builder_field->struct_field.offset = builder->offset;

  builder_field->next = builder->field_list;
  builder->field_list = builder_field;

  builder->offset += size;
  builder->field_count++;

  return &builder_field->struct_field;
}

Descriptor *
struct_end(
  Struct_Builder *builder
) {
  assert(builder->field_count);

  Descriptor *result = temp_allocate(Descriptor);
  Descriptor_Struct_Field *field_list = temp_allocate_array(
    Descriptor_Struct_Field, builder->field_count
  );

  Struct_Builder_Field *field = builder->field_list;
  u64 index = builder->field_count - 1;
  while (field) {
    field_list[index--] = field->struct_field;
    field = field->next;
  }
  result->type = Descriptor_Type_Struct;
  result->struct_ = (const Descriptor_Struct) {
    .field_list = field_list,
    .field_count = builder->field_count,
  };

  return result;
}

Value_Overload *
ensure_memory(
  Value_Overload *overload
) {
  Operand operand = overload->operand;
  if (operand.type == Operand_Type_Memory_Indirect) return overload;
  Value_Overload *result = temp_allocate(Value_Overload);
  if (overload->descriptor->type != Descriptor_Type_Pointer) assert(!"Not implemented");
  if (overload->operand.type != Operand_Type_Register) assert(!"Not implemented");
  *result = (const Value_Overload) {
    .descriptor = overload->descriptor->pointer_to,
    .operand = {
      .type = Operand_Type_Memory_Indirect,
      .indirect = {
        .reg = overload->operand.reg,
        .displacement = 0,
      },
    },
  };
  return result;
}

Value *
struct_get_field(
  Value *struct_value,
  const char *name
) {
  Value_Overload *raw_overload = maybe_get_if_single_overload(struct_value);
  assert(raw_overload);
  Value_Overload *struct_overload = ensure_memory(raw_overload);
  Descriptor *descriptor = struct_overload->descriptor;
  assert(descriptor->type == Descriptor_Type_Struct);
  for (s32 i = 0; i < descriptor->struct_.field_count; ++i) {
    Descriptor_Struct_Field *field = &descriptor->struct_.field_list[i];
    if (strcmp(field->name, name) == 0) {
      Value_Overload *result = temp_allocate(Value_Overload);
      Operand operand = struct_overload->operand;
      // FIXME support more operands
      assert(operand.type == Operand_Type_Memory_Indirect);
      operand.indirect.displacement += field->offset;
      *result = (const Value_Overload) {
        .descriptor = field->descriptor,
        .operand = operand,
      };
      return single_overload_value(result);
    }
  }

  assert(!"Could not find a field with specified name");
  return 0;
}


Buffer function_buffer;

Value *
maybe_cast_to_tag(
  Function_Builder *builder,
  const char *name,
  Value *value
) {
  Value_Overload *overload = maybe_get_if_single_overload(value);
  assert(overload);
  assert(overload->descriptor->type == Descriptor_Type_Pointer);
  Descriptor *descriptor = overload->descriptor->pointer_to;

  // FIXME
  assert(overload->operand.type == Operand_Type_Register);
  Value_Overload *tag_overload = temp_allocate(Value_Overload);
  *tag_overload = (const Value_Overload) {
    .descriptor = &descriptor_s64,
    .operand = {
      .type = Operand_Type_Memory_Indirect,
      .indirect = {
        .reg = overload->operand.reg,
        .displacement = 0,
      },
    },
  };

  s64 count = descriptor->tagged_union.struct_count;
  for (s32 i = 0; i < count; ++i) {
    Descriptor_Struct *struct_ = &descriptor->tagged_union.struct_list[i];
    if (strcmp(struct_->name, name) == 0) {

      Descriptor *constructor_descriptor = temp_allocate(Descriptor);
      *constructor_descriptor = (const Descriptor) {
        .type = Descriptor_Type_Struct,
        .struct_ = *struct_,
      };
      Descriptor *pointer_descriptor = descriptor_pointer_to(constructor_descriptor);
      Value_Overload *result_overload = temp_allocate(Value_Overload);
      *result_overload = (const Value_Overload) {
        .descriptor = pointer_descriptor,
        .operand = rbx,
      };

      move_value(builder, result_overload,  maybe_get_if_single_overload(value_from_s64(0)));

      Value *comparison = compare(
        builder, Compare_Equal, single_overload_value(tag_overload), value_from_s64(i)
      );
      Value *result_value = single_overload_value(result_overload);
      IfBuilder(builder, comparison) {
        move_value(builder, result_overload, overload);
        Value *sum = plus(builder, result_value, value_from_s64(sizeof(s64)));
        move_value(builder, result_overload, maybe_get_if_single_overload(sum));
      }
      return result_value;
    }
  }
  assert(!"Could not find specified name in the tagged union");
  return 0;
}

typedef struct {
  int64_t x;
  int64_t y;
} Point;

Point test() {
  return (Point){42, 84};
}


spec("mass") {

  before() {
    temp_buffer = make_buffer(1024 * 1024, PAGE_READWRITE);
  }

  before_each() {
    function_buffer = make_buffer(128 * 1024, PAGE_EXECUTE_READWRITE);
    buffer_reset(&temp_buffer);
  }

  after_each() {
    free_buffer(&function_buffer);
  }

  it("should support returning structs larger than 64 bits on the stack") {
    Struct_Builder struct_builder = struct_begin();
    struct_add_field(&struct_builder, &descriptor_s64, "x");
    struct_add_field(&struct_builder, &descriptor_s64, "y");
    Descriptor *point_struct_descriptor = struct_end(&struct_builder);

    Value_Overload *return_overload = temp_allocate(Value_Overload);
    *return_overload = (Value_Overload) {
      .descriptor = point_struct_descriptor,
      .operand = {
        .type = Operand_Type_Memory_Indirect,
        .indirect = {
          .reg = rsp.reg,
          .displacement = 0,
        },
      },
    };

    Descriptor *c_test_fn_descriptor = temp_allocate(Descriptor);
    *c_test_fn_descriptor = (Descriptor){
      .type = Descriptor_Type_Function,
      .function = {
        .argument_list = 0,
        .argument_count = 0,
        .returns = return_overload,
        .frozen = false,
      },
    };
    Value_Overload *c_test_fn_overload = temp_allocate(Value_Overload);
    *c_test_fn_overload = (Value_Overload) {
      .descriptor = c_test_fn_descriptor,
      .operand = imm64((s64)test),
    };

    Value *c_test_fn_value = single_overload_value(c_test_fn_overload);

    Function(checker_value) {
      Value *test_result = call_function_value(&builder_, c_test_fn_value, 0, 0);
      Value *x = struct_get_field(test_result, "x");
      Return(x);
    }

    fn_type_void_to_s64 checker = value_as_function(checker_value, fn_type_void_to_s64);
    check(checker() == 42);
  }

  it("should support RIP-relative addressing") {
    buffer_append_s32(&function_buffer, 42);
    Value_Overload rip_overload = {
      .descriptor = &descriptor_s32,
      .operand = {
        .type = Operand_Type_RIP_Relative,
        .imm64 = (s64) function_buffer.memory,
      },
    };
    Value *rip_value = single_overload_value(&rip_overload);

    Function(checker_value) {
      Return(rip_value);
    }
    fn_type_void_to_s32 checker = value_as_function(checker_value, fn_type_void_to_s32);
    check(checker() == 42);
  }

  it("should support sizeof operator on values") {
    Value *sizeof_s32 = SizeOf(value_from_s32(0));
    Value_Overload *overload = maybe_get_if_single_overload(sizeof_s32);
    check(overload);
    check(overload->operand.type == Operand_Type_Immediate_32);
    check(overload->operand.imm32 == 4);
  }

  it("should support sizeof operator on descriptors") {
    Value *sizeof_s32 = SizeOfDescriptor(&descriptor_s32);
    Value_Overload *overload = maybe_get_if_single_overload(sizeof_s32);
    check(overload);
    check(overload->operand.type == Operand_Type_Immediate_32);
    check(overload->operand.imm32 == 4);
  }

  it("should support reflection on structs") {
    Struct_Builder struct_builder = struct_begin();
    struct_add_field(&struct_builder, &descriptor_s32, "x");
    struct_add_field(&struct_builder, &descriptor_s32, "y");
    Descriptor *point_struct_descriptor = struct_end(&struct_builder);

    Function(field_count) {
      Value_Overload *overload = maybe_get_if_single_overload(
        fn_reflect(&builder_, point_struct_descriptor)
      );
      Stack(struct_, &descriptor_struct_reflection, overload);
      Return(struct_get_field(struct_, "field_count"));
    }
    s32 count = value_as_function(field_count, fn_type_void_to_s32)();
    check(count == 2);
  }

  it("should support tagged unions") {
    Descriptor_Struct_Field some_fields[] = {
      {
        .name = "value",
        .descriptor = &descriptor_s64,
        .offset = 0,
      },
    };

    Descriptor_Struct constructors[] = {
      {
        .name = "None",
        .field_list = 0,
        .field_count = 0,
      },
      {
        .name = "Some",
        .field_list = some_fields,
        .field_count = static_array_size(some_fields),
      },
    };

    Descriptor option_s64_descriptor = {
      .type = Descriptor_Type_Tagged_Union,
      .tagged_union = {
        .struct_list = constructors,
        .struct_count = static_array_size(constructors),
      },
    };


    Function(with_default_value) {
      Arg(option_value, descriptor_pointer_to(&option_s64_descriptor));
      Arg_s64(default_value);
      Value *some = maybe_cast_to_tag(&builder_, "Some", option_value);
      If(some) {
        Value *value = struct_get_field(some, "value");
        Return(value);
      }
      Return(default_value);
    }

    fn_type_voidp_s64_to_s64 with_default =
      value_as_function(with_default_value, fn_type_voidp_s64_to_s64);
    struct { s64 tag; s64 maybe_value; } test_none = {0};
    struct { s64 tag; s64 maybe_value; } test_some = {1, 21};
    check(with_default(&test_none, 42) == 42);
    check(with_default(&test_some, 42) == 21);
  }

  it("should say that the types are the same for integers of the same size") {
    check(same_type(&descriptor_s32, &descriptor_s32));
  }

  it("should say that the types are not the same for integers of different sizes") {
    check(!same_type(&descriptor_s64, &descriptor_s32));
  }

  it("should say that pointer and a s64 are different types") {
    check(!same_type(&descriptor_s64, descriptor_pointer_to(&descriptor_s64)));
  }

  it("should say that (s64 *) is not the same as (s32 *)") {
    check(!same_type(descriptor_pointer_to(&descriptor_s32), descriptor_pointer_to(&descriptor_s64)));
  }

  it("should say that (s64[2]) is not the same as (s32[2])") {
    check(!same_type(
      descriptor_array_of(&descriptor_s32, 2),
      descriptor_array_of(&descriptor_s64, 2)
    ));
  }

  it("should say that (s64[10]) is not the same as (s64[2])") {
    check(!same_type(
      descriptor_array_of(&descriptor_s64, 10),
      descriptor_array_of(&descriptor_s64, 2)
    ));
  }

  it("should support polymorphic values") {
    Value_Overload *a = maybe_get_if_single_overload(value_from_s32(0));
    Value_Overload *b = maybe_get_if_single_overload(value_from_s64(0));

    Value_Overload *overload_list[] = {a, b};

    Value overload = {
      .overload_list = overload_list,
      .overload_count = 2,
    };

    Value_Overload_Pair *pair = get_matching_values(&overload, value_from_s64(0));
    check(same_overload_type(pair->a, b));
  }

  it("should say that structs are different if their descriptors are different pointers") {
    Struct_Builder struct_builder = struct_begin();
    struct_add_field(&struct_builder, &descriptor_s32, "x");
    Descriptor *a = struct_end(&struct_builder);

    struct_builder = struct_begin();
    struct_add_field(&struct_builder, &descriptor_s32, "x");
    Descriptor *b = struct_end(&struct_builder);

    check(same_type(a, a));
    check(!same_type(a, b));
  }

  it("should support structs") {
    // struct Size { s8 width; s32 height; };

    Struct_Builder struct_builder = struct_begin();

    struct_add_field(&struct_builder, &descriptor_s32, "width");
    struct_add_field(&struct_builder, &descriptor_s32, "height");
    struct_add_field(&struct_builder, &descriptor_s32, "dummy");

    Descriptor *size_struct_descriptor = struct_end(&struct_builder);

    Descriptor *size_struct_pointer_descriptor = descriptor_pointer_to(size_struct_descriptor);

    Function(area) {
      Arg(size_struct, size_struct_pointer_descriptor);
      Return(Multiply(
        struct_get_field(size_struct, "width"),
        struct_get_field(size_struct, "height")
      ));
    }

    struct { s32 width; s32 height; s32 dummy; } size = { 10, 42 };
    s32 result = value_as_function(area, fn_type_voidp_to_s32)(&size);
    check(result == 420);
    check(sizeof(size) == descriptor_byte_size(size_struct_descriptor));
  }

  it("should add 1 to all numbers in an array") {
    s32 array[] = {1, 2, 3};

    Descriptor array_descriptor = {
      .type = Descriptor_Type_Fixed_Size_Array,
      .array = {
        .item = &descriptor_s32,
        .length = 3,
      },
    };

    Descriptor array_pointer_descriptor = {
      .type = Descriptor_Type_Pointer,
      .pointer_to = &array_descriptor,
    };

    Function(increment) {
      Arg(arr, &array_pointer_descriptor);

      Stack_s32(index, maybe_get_if_single_overload(value_from_s32(0)));
      Value_Overload *index_overload = maybe_get_if_single_overload(index);

      Stack(temp, &array_pointer_descriptor, maybe_get_if_single_overload(arr));

      u32 item_byte_size = descriptor_byte_size(array_pointer_descriptor.pointer_to->array.item);
      Loop {
        // TODO check that the descriptor in indeed an array
        s32 length = (s32)array_pointer_descriptor.pointer_to->array.length;
        If(Greater(index, value_from_s32(length))) {
          Break;
        }

        Value_Overload *temp_overload = maybe_get_if_single_overload(temp);
        Value_Overload *reg_a = value_register_for_descriptor(Register_A,
          temp_overload->descriptor);
        move_value(&builder_, reg_a, temp_overload);

        Operand pointer = {
          .type = Operand_Type_Memory_Indirect,
          .byte_size = item_byte_size,
          .indirect = (const Operand_Memory_Indirect) {
            .reg = rax.reg,
            .displacement = 0,
          }
        };
        encode(&builder_, (Instruction) {inc, {pointer, 0, 0}});
        encode(&builder_, (Instruction) {add, {temp_overload->operand, imm32(item_byte_size), 0}});

        encode(&builder_, (Instruction) {inc, {index_overload->operand, 0, 0}});
      }

    }
    value_as_function(increment, fn_type_s32p_to_void)(array);

    check(array[0] == 2);
    check(array[1] == 3);
    check(array[2] == 4);
  }
}
