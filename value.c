#include "value.h"
#include "function.h"
#include "source.h"

#ifdef _WIN32
#include "win32_runtime.h"
#endif

static inline Label *
program_get_label(
  Program *program,
  Label_Index label
) {
  return dyn_array_get(program->labels, label.value);
}

inline bool
same_value_type(
  Value *a,
  Value *b
);

bool
same_type(
  Descriptor *a,
  Descriptor *b
) {
  if (a->tag != b->tag) return false;
  switch(a->tag) {
    case Descriptor_Tag_Pointer: {
      if (
        a->Pointer.to->tag == Descriptor_Tag_Fixed_Size_Array &&
        same_type(a->Pointer.to->Fixed_Size_Array.item, b->Pointer.to)
      ) return true;
      if (
        b->Pointer.to->tag == Descriptor_Tag_Fixed_Size_Array &&
        same_type(b->Pointer.to->Fixed_Size_Array.item, a->Pointer.to)
      ) return true;
      if (
        a->Pointer.to->tag == Descriptor_Tag_Void ||
        b->Pointer.to->tag == Descriptor_Tag_Void
      ) {
        return true;
      }
      return same_type(a->Pointer.to, b->Pointer.to);
    }
    case Descriptor_Tag_Fixed_Size_Array: {
      return same_type(a->Fixed_Size_Array.item, b->Fixed_Size_Array.item) &&
        a->Fixed_Size_Array.length == b->Fixed_Size_Array.length;
    }
    case Descriptor_Tag_Void:
    case Descriptor_Tag_Opaque:
    case Descriptor_Tag_Struct: {
      return a == b;
    }
    case Descriptor_Tag_Function: {
      if (!same_type(a->Function.returns->descriptor, b->Function.returns->descriptor)) {
        return false;
      }
      if (dyn_array_length(a->Function.arguments) != dyn_array_length(b->Function.arguments)) {
        return false;
      }
      for (u64 i = 0; i < dyn_array_length(a->Function.arguments); ++i) {
        if (!same_value_type(
          *dyn_array_get(a->Function.arguments, i),
          *dyn_array_get(b->Function.arguments, i)
        )) {
          return false;
        }
      }
      return true;
    }
    case Descriptor_Tag_Any:
    default: {
      assert(!"Unsupported descriptor type");
      return false;
    }
  }
}

u32
descriptor_alignment(
  Descriptor *descriptor
) {
  if (descriptor->tag == Descriptor_Tag_Fixed_Size_Array) {
    return descriptor_alignment(descriptor->Fixed_Size_Array.item);
  }
  return descriptor_byte_size(descriptor);
}

u32
struct_byte_size(
  const Descriptor_Struct *Struct
) {
  s64 count = dyn_array_length(Struct->fields);
  assert(count);
  u32 alignment = 0;
  u32 raw_size = 0;
  for (s32 i = 0; i < count; ++i) {
    Descriptor_Struct_Field *field = dyn_array_get(Struct->fields, i);
    u32 field_alignment = descriptor_alignment(field->descriptor);
    alignment = u32_max(alignment, field_alignment);
    bool is_last_field = i == count - 1;
    u32 field_size_with_alignment = u32_max(field_alignment, descriptor_byte_size(field->descriptor));
    if (is_last_field) {
      raw_size = field->offset + field_size_with_alignment;
    }
  }
  return s32_align(raw_size, alignment);
}

u32
descriptor_byte_size(
  const Descriptor *descriptor
) {
  assert(descriptor);
  switch(descriptor->tag) {
    case Descriptor_Tag_Void: {
      return 0;
    }
    case Descriptor_Tag_Struct: {
      return struct_byte_size(&descriptor->Struct);
    }
    case Descriptor_Tag_Opaque: {
      u64 size_of_byte = 8;
      return u64_to_u32((descriptor->Opaque.bit_size + (size_of_byte - 1)) / size_of_byte);
    }
    case Descriptor_Tag_Fixed_Size_Array: {
      return descriptor_byte_size(descriptor->Fixed_Size_Array.item) *
        descriptor->Fixed_Size_Array.length;
    }
    case Descriptor_Tag_Pointer:
    case Descriptor_Tag_Function: {
      return 8;
    }
    case Descriptor_Tag_Any: {
      panic("Trying to get byte size of an Any descriptor");
      break;
    }
    default: {
      assert(!"Unknown Descriptor Type");
    }
  }
  return 0;
}

Source_Position
source_file_offset_to_position(
  const Source_File *file,
  u64 offset
) {
  // Binary search in lines
  s64 left_bound = 0;
  s64 right_bound = dyn_array_length(file->line_ranges) - 1;
  s64 line_index = 0;
  while (left_bound <= right_bound) {
    line_index = left_bound + (right_bound - left_bound) / 2;
    Range_u64 *line = dyn_array_get(file->line_ranges, line_index);
    if (offset < line->from) {
      right_bound = line_index - 1;
    } else if (offset >= line->to) {
      left_bound = line_index + 1;
    } else {
      break;
    }
  }

  u64 column = offset - dyn_array_get(file->line_ranges, line_index)->from;
  return (Source_Position) {
    .line = line_index + 1,
    .column = column,
  };
}

void
source_range_print_start_position(
  const Source_Range *source_range
) {
  if (!source_range->file || !dyn_array_is_initialized(source_range->file->line_ranges)) {
    printf(":(0:0)\n");
    return;
  }
  Source_Position from_position =
    source_file_offset_to_position(source_range->file, source_range->offsets.from);
  slice_print(source_range->file->path);
  printf(":(%" PRIu64 ":%" PRIu64 ")\n", from_position.line, from_position.column);
}

static inline Descriptor *
operand_immediate_memory_as_descriptor(
  const Operand *operand
) {
  assert(operand->tag == Operand_Tag_Immediate);
  assert(operand->byte_size == sizeof(Descriptor));
  return operand->Immediate.memory;
}

#define OPERAND_IMMEDIATE_CAST(_TYPE_)\
  static inline _TYPE_\
  operand_immediate_memory_as_##_TYPE_(\
    const Operand *operand\
  ) {\
    assert(operand->byte_size == sizeof(_TYPE_));\
    return *((_TYPE_ *)operand->Immediate.memory);\
  }

OPERAND_IMMEDIATE_CAST(u8)
OPERAND_IMMEDIATE_CAST(s8)
OPERAND_IMMEDIATE_CAST(u16)
OPERAND_IMMEDIATE_CAST(s16)
OPERAND_IMMEDIATE_CAST(u32)
OPERAND_IMMEDIATE_CAST(s32)
OPERAND_IMMEDIATE_CAST(u64)
OPERAND_IMMEDIATE_CAST(s64)

s64
operand_immediate_value_up_to_s64(
  const Operand *operand
) {
  switch(operand->byte_size) {
    case 1: return operand_immediate_memory_as_s8(operand);
    case 2: return operand_immediate_memory_as_s16(operand);
    case 4: return operand_immediate_memory_as_s32(operand);
    case 8: return operand_immediate_memory_as_s64(operand);
    default: {
      panic("Unsupported integer immediate size");
      return 0;
    }
  }
}

u64
operand_immediate_value_up_to_u64(
  const Operand *operand
) {
  switch(operand->byte_size) {
    case 1: return operand_immediate_memory_as_u8(operand);
    case 2: return operand_immediate_memory_as_u16(operand);
    case 4: return operand_immediate_memory_as_u32(operand);
    case 8: return operand_immediate_memory_as_u64(operand);
    default: {
      panic("Unsupported integer immediate size");
      return 0;
    }
  }
}

void
print_operand(
  const Operand *operand
) {
  switch (operand->tag) {
    case Operand_Tag_None: {
      printf("_");
      break;
    }
    case Operand_Tag_Any: {
      printf("any");
      break;
    }
    case Operand_Tag_Eflags: {
      printf("eflags");
      break;
    }
    case Operand_Tag_Register: {
      u32 bits = operand->byte_size * 8;
      printf("r%d", bits);
      break;
    }
    case Operand_Tag_Xmm: {
      u32 bits = operand->byte_size * 8;
      printf("xmm%d", bits);
      break;
    }
    case Operand_Tag_Immediate: {
      switch(operand->byte_size) {
        case 1: {
          printf("imm8(0x%02x)", operand_immediate_memory_as_u8(operand));
          break;
        }
        case 2: {
          printf("imm16(0x%04x)", operand_immediate_memory_as_u16(operand));
          break;
        }
        case 4: {
          printf("imm32(0x%08x)", operand_immediate_memory_as_u32(operand));
          break;
        }
        case 8: {
          printf("imm64(0x%016" PRIx64 ")", operand_immediate_memory_as_u64(operand));
          break;
        }
        default: {
          panic("Unsupported immediate size when printing");
          break;
        }
      }
      break;
    }
    case Operand_Tag_Memory: {
      // TODO print better info
      u32 bits = operand->byte_size * 8;
      printf("m%d", bits);
      break;
    }
    default: {
      printf("<unknown>");
      break;
    }
  }
}

#define define_register(reg_name, reg_index, reg_byte_size) \
const Operand reg_name = { \
  .tag = Operand_Tag_Register, \
  .byte_size = (reg_byte_size), \
  .Register = {.index = (reg_index)}, \
};
define_register(al, 0b0000, 1);

define_register(rax, 0b0000, 8);
define_register(rcx, 0b0001, 8);
define_register(rdx, 0b0010, 8);
define_register(rbx, 0b0011, 8);
define_register(rsp, 0b0100, 8);
define_register(rbp, 0b0101, 8);
define_register(rsi, 0b0110, 8);
define_register(rdi, 0b0111, 8);

define_register(eax, 0b0000, 4);
define_register(ecx, 0b0001, 4);
define_register(edx, 0b0010, 4);
define_register(ebx, 0b0011, 4);
define_register(esp, 0b0100, 4);
define_register(ebp, 0b0101, 4);
define_register(esi, 0b0110, 4);
define_register(edi, 0b0111, 4);

define_register(r8,  0b1000, 8);
define_register(r9,  0b1001, 8);
define_register(r10, 0b1010, 8);
define_register(r11, 0b1011, 8);
define_register(r12, 0b1100, 8);
define_register(r13, 0b1101, 8);
define_register(r14, 0b1110, 8);
define_register(r15, 0b1111, 8);

define_register(r8d,  0b1000, 4);
define_register(r9d,  0b1001, 4);
define_register(r10d, 0b1010, 4);
define_register(r11d, 0b1011, 4);
define_register(r12d, 0b1100, 4);
define_register(r13d, 0b1101, 4);
define_register(r14d, 0b1110, 4);
define_register(r15d, 0b1111, 4);
#undef define_register

#define define_xmm_register(reg_name, reg_index) \
const Operand reg_name##_32 = { \
  .tag = Operand_Tag_Xmm, \
  .byte_size = 4, \
  .Register = {.index = (reg_index)}, \
};\
const Operand reg_name##_64 = { \
  .tag = Operand_Tag_Xmm, \
  .byte_size = 8, \
  .Register = {.index = (reg_index)}, \
};
define_xmm_register(xmm0, 0b000);
define_xmm_register(xmm1, 0b001);
define_xmm_register(xmm2, 0b010);
define_xmm_register(xmm3, 0b011);
define_xmm_register(xmm4, 0b100);
define_xmm_register(xmm5, 0b101);
define_xmm_register(xmm6, 0b110);
define_xmm_register(xmm7, 0b111);
#undef define_xmm_register

static inline Label_Index
make_label(
  Program *program,
  Section *section
) {
  Label_Index index = {dyn_array_length(program->labels)};
  dyn_array_push(program->labels, (Label) {.section = section});
  return index;
}

static inline Operand
data_label32(
  Label_Index label_index,
  u32 byte_size
) {
  return (const Operand) {
    .tag = Operand_Tag_Memory,
    .byte_size = byte_size,
    .Memory.location = {
      .tag = Memory_Location_Tag_Instruction_Pointer_Relative,
      .Instruction_Pointer_Relative.label_index = label_index
    }
  };
}

static inline Operand
code_label32(
  Label_Index label_index
) {
  return (const Operand) {
    .tag = Operand_Tag_Memory,
    // FIXME this is set at 4 as otherwise current encoder is unhappy
    //       about the size mismatch. It should be zero instead.
    .byte_size = 4,
    .Memory.location = {
      .tag = Memory_Location_Tag_Instruction_Pointer_Relative,
      .Instruction_Pointer_Relative.label_index = label_index,
    }
  };
}

static inline Operand
imm8(
  const Allocator *allocator,
  u8 value
) {
  return (Operand) {
    .tag = Operand_Tag_Immediate,
    .byte_size = sizeof(value),
    .Immediate.memory = memcpy(
      allocator_allocate_bytes(allocator, sizeof(value), sizeof(value)), &value, sizeof(value)
    ),
  };
}

static inline Operand
imm16(
  const Allocator *allocator,
  u16 value
) {
  return (Operand) {
    .tag = Operand_Tag_Immediate,
    .byte_size = sizeof(value),
    .Immediate.memory = memcpy(
      allocator_allocate_bytes(allocator, sizeof(value), sizeof(value)), &value, sizeof(value)
    ),
  };
}

static inline Operand
imm32(
  const Allocator *allocator,
  u32 value
) {
  return (Operand) {
    .tag = Operand_Tag_Immediate,
    .byte_size = sizeof(value),
    .Immediate.memory = memcpy(
      allocator_allocate_bytes(allocator, sizeof(value), sizeof(value)), &value, sizeof(value)
    ),
  };
}

static inline Operand
imm64(
  const Allocator *allocator,
  u64 value
) {
  return (Operand) {
    .tag = Operand_Tag_Immediate,
    .byte_size = sizeof(value),
    .Immediate.memory = memcpy(
      allocator_allocate_bytes(allocator, sizeof(value), sizeof(value)), &value, sizeof(value)
    ),
  };
}

static inline Operand
imm_auto_8_or_32(
  const Allocator *allocator,
  s64 value
) {
  if (s64_fits_into_s8(value)) {
    return imm8(allocator, (s8) value);
  }
  if (s64_fits_into_s32(value)) {
    return imm32(allocator, (s32) value);
  }
  panic("Operand is does not fit into either s8 or s32");
  return (Operand){0};
}

static inline Operand
imm_auto(
  const Allocator *allocator,
  s64 value
) {
  if (s64_fits_into_s8(value)) {
    return imm8(allocator, (s8) value);
  }
  if (s64_fits_into_s16(value)) {
    return imm16(allocator, (s16) value);
  }
  if (s64_fits_into_s32(value)) {
    return imm32(allocator, (s32) value);
  }
  return imm64(allocator, value);
}

static inline Operand
stack(
  s32 offset,
  u32 byte_size
) {
  assert(byte_size);
  return (const Operand) {
    .tag = Operand_Tag_Memory,
    .byte_size = byte_size,
    .Memory.location = {
      .tag = Memory_Location_Tag_Indirect,
      .Indirect = {
        .base_register = Register_SP,
        .offset = offset,
      }
    }
  };
}

Descriptor *
descriptor_struct_make(
  Allocator *allocator
) {
  Descriptor *descriptor = allocator_allocate(allocator, Descriptor);
  *descriptor = (Descriptor) {
    .tag = Descriptor_Tag_Struct,
    .Struct = {
      .fields = dyn_array_make(Array_Descriptor_Struct_Field),
    }
  };
  return descriptor;
}

void
descriptor_struct_add_field(
  Descriptor *struct_descriptor,
  Descriptor *field_descriptor,
  Slice field_name
) {
  u32 offset = 0;
  for (u64 i = 0; i < dyn_array_length(struct_descriptor->Struct.fields); ++i) {
    Descriptor_Struct_Field *field = dyn_array_get(struct_descriptor->Struct.fields, i);
    u32 size = descriptor_byte_size(field->descriptor);
    offset = u32_align(offset, size);
    offset += size;
  }

  u32 size = descriptor_byte_size(field_descriptor);
  offset = u32_align(offset, size);
  dyn_array_push(struct_descriptor->Struct.fields, (Descriptor_Struct_Field) {
    .name = field_name,
    .descriptor = field_descriptor,
    .offset = offset,
  });
}

static inline void
register_bitset_set(
  u64 *bitset,
  Register reg
) {
  *bitset |= 1llu << reg;
}

static inline void
register_bitset_unset(
  u64 *bitset,
  Register reg
) {
  *bitset &= ~(1llu << reg);
}

static inline bool
register_bitset_get(
  u64 bitset,
  Register reg
) {
  return !!(bitset & (1llu << reg));
}

static inline bool
operand_is_label(
  const Operand *operand
) {
  return operand->tag == Operand_Tag_Memory
    && operand->Memory.location.tag == Memory_Location_Tag_Instruction_Pointer_Relative;
}

static inline bool
operand_is_register_or_memory(
  Operand *operand
) {
  return operand->tag == Operand_Tag_Register || operand->tag == Operand_Tag_Memory;
}

static inline bool
operand_equal(
  const Operand *a,
  const Operand *b
) {
  if (a->tag != b->tag) return false;
  if (a->byte_size != b->byte_size) return false;
  switch(a->tag) {
    case Operand_Tag_Eflags: {
      return a->Eflags.compare_type == b->Eflags.compare_type;
    }
    case Operand_Tag_Immediate: {
      return !memcmp(a->Immediate.memory, b->Immediate.memory, a->byte_size);
    }
    case Operand_Tag_Memory: {
      const Memory_Location *a_location = &a->Memory.location;
      const Memory_Location *b_location = &b->Memory.location;
      if (a_location->tag != b_location->tag) return false;
      switch(a_location->tag) {
        case Memory_Location_Tag_Instruction_Pointer_Relative: {
          return a_location->Instruction_Pointer_Relative.label_index.value
            == b_location->Instruction_Pointer_Relative.label_index.value;
        }
        case Memory_Location_Tag_Indirect: {
          return (
            a_location->Indirect.base_register == b_location->Indirect.base_register &&
            a_location->Indirect.maybe_index_register.has_value == b_location->Indirect.maybe_index_register.has_value &&
            a_location->Indirect.maybe_index_register.index == b_location->Indirect.maybe_index_register.index &&
            a_location->Indirect.offset == b_location->Indirect.offset
          );
          break;
        }
      }
      panic("Internal Error: Unexpected Memory_Location_Tag");
      return false;
    }
    case Operand_Tag_Xmm:
    case Operand_Tag_Register: {
      return a->Register.index == b->Register.index;
    }
    case Operand_Tag_Any: {
      return false; // Is this the semantics I want though?
    }
    case Operand_Tag_None: {
      return true;
    }
  }
  panic("Unknown operand type");
  return false;
}

bool
instruction_equal(
  const Instruction *a,
  const Instruction *b
) {
  if (a->type != b->type) return false;
  switch(a->type) {
    case Instruction_Type_Assembly: {
      if (a->assembly.mnemonic != b->assembly.mnemonic) return false;
      for (u64 i = 0; i < countof(a->assembly.operands); ++i) {
        if (!operand_equal(&a->assembly.operands[i], &b->assembly.operands[i])) {
          return false;
        }
      }
      break;
    }
    case Instruction_Type_Label: {
      return a->label.value == b->label.value;
    }
    case Instruction_Type_Bytes: {
      if (a->Bytes.length != b->Bytes.length) return false;
      return !!memcmp(a->Bytes.memory, b->Bytes.memory, a->Bytes.length);
    }
  }
  return true;
}

Value *
value_from_compare_internal(
  Compiler_Source_Location compiler_source_location,
  Allocator *allocator,
  Compare_Type compare_type
) {
  Value *result = allocator_allocate(allocator, Value);
  *result = (Value) {
    // TODO consider adding explicit boolean descriptor type
    .descriptor = &descriptor_s8,
    .operand = {
      .tag = Operand_Tag_Eflags,
      .byte_size = 1,
      .Eflags = {.compare_type = compare_type },
    },
    .compiler_source_location = compiler_source_location,
  };
  return result;
}

#define value_from_compare(...) value_from_compare_internal(COMPILER_SOURCE_LOCATION, __VA_ARGS__)

Value *
value_global_internal(
  Compiler_Source_Location compiler_source_location,
  Compilation_Context *context,
  Descriptor *descriptor
) {
  Program *program = context->program;
  u32 byte_size = descriptor_byte_size(descriptor);
  u32 alignment = descriptor_alignment(descriptor);
  void *allocation =
    bucket_buffer_allocate_bytes(program->data_section.buffer, byte_size, alignment);
  s64 offset_in_data_section =
    bucket_buffer_pointer_to_offset(program->data_section.buffer, allocation);

  Value *result = allocator_allocate(context->allocator, Value);
  Label_Index label_index = make_label(program, &program->data_section);
  Label *label = program_get_label(program, label_index);
  label->offset_in_section = s64_to_u32(offset_in_data_section);

  *result = (Value) {
    .descriptor = descriptor,
    .operand = data_label32(label_index, byte_size),
    .compiler_source_location = compiler_source_location,
  };
  return result;
}

Value *
value_any_internal(
  Compiler_Source_Location compiler_source_location,
  const Allocator *allocator
) {
  Value *result = allocator_allocate(allocator, Value);
  *result = (Value) {
    .descriptor = &descriptor_any,
    .operand = {.tag = Operand_Tag_Any},
    .compiler_source_location = compiler_source_location,
  };
  return result;
}

#define value_any(...) value_any_internal(COMPILER_SOURCE_LOCATION, ##__VA_ARGS__)

Value *
value_from_s64_internal(
  Compiler_Source_Location compiler_source_location,
  const Allocator *allocator,
  s64 integer
) {
  Value *result = allocator_allocate(allocator, Value);
  *result = (Value) {
    .descriptor = &descriptor_s64,
    .operand = imm64(allocator, integer),
    .compiler_source_location = compiler_source_location,
  };
  return result;
}
#define value_from_s64(...) value_from_s64_internal(COMPILER_SOURCE_LOCATION, __VA_ARGS__)

Value *
value_from_s32_internal(
  Compiler_Source_Location compiler_source_location,
  const Allocator *allocator,
  s32 integer
) {
  Value *result = allocator_allocate(allocator, Value);
  *result = (const Value) {
    .descriptor = &descriptor_s32,
    .operand = imm32(allocator, integer),
    .compiler_source_location = compiler_source_location,
  };
  return result;
}

#define value_from_s32(...) value_from_s32_internal(COMPILER_SOURCE_LOCATION, __VA_ARGS__)

Value *
value_from_s16_internal(
  Compiler_Source_Location compiler_source_location,
  const Allocator *allocator,
  s16 integer
) {
  Value *result = allocator_allocate(allocator, Value);
  *result = (const Value) {
    .descriptor = &descriptor_s16,
    .operand = imm16(allocator, integer),
    .compiler_source_location = compiler_source_location,
  };
  return result;
}

#define value_from_s16(...) value_from_s16_internal(COMPILER_SOURCE_LOCATION, __VA_ARGS__)

Value *
value_from_s8_internal(
  Compiler_Source_Location compiler_source_location,
  const Allocator *allocator,
  s8 integer
) {
  Value *result = allocator_allocate(allocator, Value);
  *result = (const Value) {
    .descriptor = &descriptor_s8,
    .operand = imm8(allocator, integer),
    .compiler_source_location = compiler_source_location,
  };
  return result;
}

#define value_from_s8(...) value_from_s8_internal(COMPILER_SOURCE_LOCATION, __VA_ARGS__)

// FIXME turn these into macros

Value *
value_from_u64_internal(
  Compiler_Source_Location compiler_source_location,
  const Allocator *allocator,
  u64 integer
) {
  Value *result = allocator_allocate(allocator, Value);
  *result = (Value) {
    .descriptor = &descriptor_u64,
    .operand = imm64(allocator, integer),
    .compiler_source_location = compiler_source_location,
  };
  return result;
}
#define value_from_u64(...) value_from_u64_internal(COMPILER_SOURCE_LOCATION, __VA_ARGS__)

Value *
value_from_u32_internal(
  Compiler_Source_Location compiler_source_location,
  const Allocator *allocator,
  u32 integer
) {
  Value *result = allocator_allocate(allocator, Value);
  *result = (Value) {
    .descriptor = &descriptor_u32,
    .operand = imm32(allocator, integer),
    .compiler_source_location = compiler_source_location,
  };
  return result;
}
#define value_from_u32(...) value_from_u32_internal(COMPILER_SOURCE_LOCATION, __VA_ARGS__)

Value *
value_from_u16_internal(
  Compiler_Source_Location compiler_source_location,
  const Allocator *allocator,
  u16 integer
) {
  Value *result = allocator_allocate(allocator, Value);
  *result = (Value) {
    .descriptor = &descriptor_u16,
    .operand = imm16(allocator, integer),
    .compiler_source_location = compiler_source_location,
  };
  return result;
}
#define value_from_u16(...) value_from_u16_internal(COMPILER_SOURCE_LOCATION, __VA_ARGS__)

Value *
value_from_u8_internal(
  Compiler_Source_Location compiler_source_location,
  const Allocator *allocator,
  u8 integer
) {
  Value *result = allocator_allocate(allocator, Value);
  *result = (Value) {
    .descriptor = &descriptor_u8,
    .operand = imm8(allocator, integer),
    .compiler_source_location = compiler_source_location,
  };
  return result;
}
#define value_from_u8(...) value_from_u8_internal(COMPILER_SOURCE_LOCATION, __VA_ARGS__)



static inline Value *
value_from_signed_immediate_internal(
  Compiler_Source_Location compiler_source_location,
  const Allocator *allocator,
  s64 value
) {
  if (s64_fits_into_s8(value)) {
    return value_from_s8_internal(compiler_source_location, allocator, (s8) value);
  }
  if (s64_fits_into_s16(value)) {
    return value_from_s16_internal(compiler_source_location, allocator, (s16) value);
  }
  if (s64_fits_into_s32(value)) {
    return value_from_s32_internal(compiler_source_location, allocator, (s32) value);
  }
  return value_from_s64_internal(compiler_source_location, allocator, value);
}
#define value_from_signed_immediate(...)\
  value_from_signed_immediate_internal(COMPILER_SOURCE_LOCATION, __VA_ARGS__)

static inline Value *
value_from_unsigned_immediate_internal(
  Compiler_Source_Location compiler_source_location,
  const Allocator *allocator,
  u64 value
) {
  if (u64_fits_into_u8(value)) {
    return value_from_u8_internal(compiler_source_location, allocator, (u8) value);
  }
  if (u64_fits_into_u16(value)) {
    return value_from_u16_internal(compiler_source_location, allocator, (u16) value);
  }
  if (u64_fits_into_u32(value)) {
    return value_from_u32_internal(compiler_source_location, allocator, (u32) value);
  }
  return value_from_u64_internal(compiler_source_location, allocator, value);
}
#define value_from_unsigned_immediate(...)\
  value_from_unsigned_immediate_internal(COMPILER_SOURCE_LOCATION, __VA_ARGS__)

Value *
value_byte_size_internal(
  Compiler_Source_Location compiler_source_location,
  Allocator *allocator,
  Value *value
) {
  s32 byte_size = descriptor_byte_size(value->descriptor);
  return value_from_s32_internal(compiler_source_location, allocator, byte_size);
}
#define value_byte_size(...)\
  value_byte_size_internal(COMPILER_SOURCE_LOCATION, __VA_ARGS__)

static inline Operand
operand_register_for_descriptor(
  Register reg,
  Descriptor *descriptor
) {
  u32 byte_size = descriptor_byte_size(descriptor);
  assert(byte_size == 1 || byte_size == 2 || byte_size == 4 || byte_size == 8);

  Operand result = {
    .tag = register_is_xmm(reg) ? Operand_Tag_Xmm : Operand_Tag_Register,
    .Register.index = reg,
    .byte_size = byte_size,
  };
  return result;
}

static inline Value *
value_register_for_descriptor_internal(
  Compiler_Source_Location compiler_source_location,
  Allocator *allocator,
  Register reg,
  Descriptor *descriptor
) {
  Value *result = allocator_allocate(allocator, Value);
  *result = (Value) {
    .descriptor = descriptor,
    .operand = operand_register_for_descriptor(reg, descriptor),
    .compiler_source_location = compiler_source_location,
  };
  return result;
}
#define value_register_for_descriptor(...)\
  value_register_for_descriptor_internal(COMPILER_SOURCE_LOCATION, __VA_ARGS__)

void *
rip_value_pointer(
  Program *program,
  Value *value
) {
  assert(operand_is_label(&value->operand));
  Label *label = program_get_label(
    program, value->operand.Memory.location.Instruction_Pointer_Relative.label_index
  );
  return bucket_buffer_offset_to_pointer(label->section->buffer, label->offset_in_section);
}

Value *
value_global_c_string_from_slice_internal(
  Compiler_Source_Location compiler_source_location,
  Compilation_Context *context,
  Slice slice
) {
  s32 length = (s32)slice.length + 1;
  Descriptor *descriptor = allocator_allocate(context->allocator, Descriptor);
  *descriptor = (Descriptor) {
    .tag = Descriptor_Tag_Fixed_Size_Array,
    .Fixed_Size_Array = {
      .item = &descriptor_s8,
      .length = length,
    },
  };

  Value *string_value = value_global_internal(compiler_source_location, context, descriptor);
  s8 *memory = rip_value_pointer(context->program, string_value);
  memcpy(memory, slice.bytes, slice.length);
  memory[length - 1] = 0;
  return string_value;
}
#define value_global_c_string_from_slice(...)\
  value_global_c_string_from_slice_internal(COMPILER_SOURCE_LOCATION, __VA_ARGS__)

Descriptor *
descriptor_pointer_to(
  Allocator *allocator,
  Descriptor *descriptor
) {
  Descriptor *result = allocator_allocate(allocator, Descriptor);
  *result = (const Descriptor) {
    .tag = Descriptor_Tag_Pointer,
    .Pointer = {.to = descriptor },
  };
  return result;
}

Descriptor *
descriptor_array_of(
  Allocator *allocator,
  Descriptor *descriptor,
  u32 length
) {
  Descriptor *result = allocator_allocate(allocator, Descriptor);
  *result = (Descriptor) {
    .tag = Descriptor_Tag_Fixed_Size_Array,
    .Fixed_Size_Array = {
      .item = descriptor,
      .length = length,
    },
  };
  return result;
}

fn_type_opaque
value_as_function(
  Program *program,
  Value *value
) {
  assert(operand_is_label(&value->operand));
  assert(program->jit_buffer);
  Label *label = program_get_label(
    program, value->operand.Memory.location.Instruction_Pointer_Relative.label_index
  );
  assert(label->section == &program->code_section);
  s8 *target = program->jit_buffer->memory + label->section->base_rva + label->offset_in_section;
  return (fn_type_opaque)target;
}


bool
memory_range_equal_to_c_string(
  const void *memory_range_start,
  const void *memory_range_end,
  const char *string
) {
  s64 length = ((char *)memory_range_end) - ((char *)memory_range_start);
  s64 string_length = strlen(string);
  if (string_length != length) return false;
  return memcmp(memory_range_start, string, string_length) == 0;
}

Value *
function_next_argument_value_internal(
  Compiler_Source_Location compiler_source_location,
  Allocator *allocator,
  Descriptor_Function *function,
  Descriptor *arg_descriptor
) {
  u32 byte_size = descriptor_byte_size(arg_descriptor);
  assert(byte_size <= 8);

  u64 argument_index = dyn_array_length(function->arguments);
  // :ReturnTypeLargerThanRegister
  // If return type is larger than register, the pointer to stack location
  // where it needs to be written to is passed as the first argument
  // shifting registers for actual arguments by one
  if (descriptor_byte_size(function->returns->descriptor) > 8) {
    argument_index++;
  }
  switch (argument_index) {
    case 0: {
      Value *value = descriptor_is_float(arg_descriptor)
        ? value_register_for_descriptor_internal(compiler_source_location, allocator, Register_Xmm0, arg_descriptor)
        : value_register_for_descriptor_internal(compiler_source_location, allocator, Register_C, arg_descriptor);
      return value;
    }
    case 1: {
      Value *value = descriptor_is_float(arg_descriptor)
        ? value_register_for_descriptor_internal(compiler_source_location, allocator, Register_Xmm1, arg_descriptor)
        : value_register_for_descriptor_internal(compiler_source_location, allocator, Register_D, arg_descriptor);
      return value;
    }
    case 2: {
      Value *value = descriptor_is_float(arg_descriptor)
        ? value_register_for_descriptor_internal(compiler_source_location, allocator, Register_Xmm2, arg_descriptor)
        : value_register_for_descriptor_internal(compiler_source_location, allocator, Register_R8, arg_descriptor);
      return value;
    }
    case 3: {
      Value *value = descriptor_is_float(arg_descriptor)
        ? value_register_for_descriptor_internal(compiler_source_location, allocator, Register_Xmm3, arg_descriptor)
        : value_register_for_descriptor_internal(compiler_source_location, allocator, Register_R9, arg_descriptor);
      return value;
    }
    default: {
      s32 offset = u64_to_s32(dyn_array_length(function->arguments) * 8);
      Operand operand = stack(offset, byte_size);
      Value *value = allocator_allocate(allocator, Value);
      *value = (Value) {
        .descriptor = arg_descriptor,
        .operand = operand,
      };
      return value;
    }
  }
}
#define function_next_argument_value(...)\
  function_next_argument_value_internal(COMPILER_SOURCE_LOCATION, __VA_ARGS__)

Descriptor *
parse_c_type(
  Allocator *allocator,
  const char *range_start,
  const char *range_end
) {
  Descriptor *descriptor = 0;

  const char *start = range_start;
  for(const char *ch = range_start; ch <= range_end; ++ch) {
    if (!(*ch == ' ' || *ch == '*' || ch == range_end)) continue;
    if (start != ch) {
      if (
        memory_range_equal_to_c_string(start, ch, "char") ||
        memory_range_equal_to_c_string(start, ch, "s8")
      ) {
        descriptor = &descriptor_s8;
      } else if (
        memory_range_equal_to_c_string(start, ch, "int") ||
        memory_range_equal_to_c_string(start, ch, "s32")
      ) {
        descriptor = &descriptor_s32;
      } else if (
        memory_range_equal_to_c_string(start, ch, "void *") ||
        memory_range_equal_to_c_string(start, ch, "s64")
      ) {
        descriptor = &descriptor_s64;
      } else if (memory_range_equal_to_c_string(start, ch, "void")) {
        descriptor = &descriptor_void;
      } else if (memory_range_equal_to_c_string(start, ch, "const")) {
        // TODO support const values?
      } else {
        assert(!"Unsupported argument type");
      }
    }
    if (*ch == '*') {
      assert(descriptor);
      Descriptor *previous_descriptor = descriptor;
      descriptor = allocator_allocate(allocator, Descriptor);
      *descriptor = (const Descriptor) {
        .tag = Descriptor_Tag_Pointer,
        .Pointer = { .to = previous_descriptor },
      };
    }
    start = ch + 1;
  }
  return descriptor;
}

Value *
c_function_return_value(
  Allocator *allocator,
  const char *forward_declaration
) {
  char *ch = strchr(forward_declaration, '(');
  assert(ch);
  --ch;

  // skip whitespace before (
  for(; *ch == ' '; --ch);
  for(;
    (*ch >= 'a' && *ch <= 'z') ||
    (*ch >= 'A' && *ch <= 'Z') ||
    (*ch >= '0' && *ch <= '9') ||
    *ch == '_';
    --ch
  )
  // skip whitespace before function name
  for(; *ch == ' '; --ch);
  ++ch;
  Descriptor *descriptor = parse_c_type(allocator, forward_declaration, ch);
  assert(descriptor);
  switch(descriptor->tag) {
    case Descriptor_Tag_Void: {
      return &void_value;
    }
    case Descriptor_Tag_Function:
    case Descriptor_Tag_Pointer: {
      Value *return_value = value_register_for_descriptor(allocator, Register_A, descriptor);
      return return_value;
    }
    case Descriptor_Tag_Opaque: {
      if (descriptor_is_integer(descriptor)) {
        Value *return_value = value_register_for_descriptor(allocator, Register_A, descriptor);
        return return_value;
      }
      panic("Unknown opaque type");
      break;
    }
    case Descriptor_Tag_Any:
    case Descriptor_Tag_Fixed_Size_Array:
    case Descriptor_Tag_Struct:
    default: {
      assert(!"Unsupported return type");
    }
  }
  return 0;
}

Descriptor *
c_function_descriptor(
  Allocator *allocator,
  const char *forward_declaration
) {
  Descriptor *descriptor = allocator_allocate(allocator, Descriptor);
  *descriptor = (const Descriptor) {
    .tag = Descriptor_Tag_Function,
    .Function = {
      .arguments = dyn_array_make(Array_Value_Ptr, .allocator = allocator),
      .returns = 0,
    },
  };

  descriptor->Function.returns = c_function_return_value(allocator, forward_declaration);
  char *ch = strchr(forward_declaration, '(');
  assert(ch);
  ch++;

  char *start = ch;
  Descriptor *argument_descriptor = 0;
  for (; *ch; ++ch) {
    if (*ch == ',' || *ch == ')') {
      if (start != ch) {
        argument_descriptor = parse_c_type(allocator, start, ch);
        // support for foo(void) fn signature
        if (
          dyn_array_length(descriptor->Function.arguments) == 0 &&
          argument_descriptor->tag == Descriptor_Tag_Void
        ) {
          assert(*ch == ')');
          break;
        }
        Value *arg_value =
          function_next_argument_value(allocator, &descriptor->Function, argument_descriptor);
        dyn_array_push(descriptor->Function.arguments, arg_value);
        assert(argument_descriptor);
      }
      start = ch + 1;
    }
  }

  return descriptor;
}

Value *
c_function_value(
  Allocator *allocator,
  const char *forward_declaration,
  fn_type_opaque fn
) {
  Value *result = allocator_allocate(allocator, Value);
  *result = (const Value) {
    .descriptor = c_function_descriptor(allocator, forward_declaration),
    .operand = imm64(allocator, (u64) fn),
  };
  return result;
}

Program *
program_init(
  Allocator *allocator,
  Program *program
) {
  *program = (Program) {
    .labels = dyn_array_make(Array_Label, .capacity = 128, .allocator = allocator),
    .patch_info_array = dyn_array_make(Array_Label_Location_Diff_Patch_Info, .capacity = 128, .allocator = allocator),
    .import_libraries = dyn_array_make(Array_Import_Library, .capacity = 16, .allocator = allocator),
    .functions = dyn_array_make(Array_Function_Builder, .capacity = 16, .allocator = allocator),
    .global_scope = scope_make(allocator, 0),
    .data_section = {
      .buffer = bucket_buffer_make(.allocator = allocator_system),
      .permissions = Section_Permissions_Read | Section_Permissions_Write,
    },
    .code_section = {
      .buffer = bucket_buffer_make(.allocator = allocator_system),
      .permissions = Section_Permissions_Execute,
    },
  };

  scope_define(program->global_scope, slice_literal("[]"), (Scope_Entry) {
    .type = Scope_Entry_Type_Operator,
    .Operator = { .precedence = 20, .fixity = Operator_Fixity_Postfix }
  });
  scope_define(program->global_scope, slice_literal("()"), (Scope_Entry) {
    .type = Scope_Entry_Type_Operator,
    .Operator = { .precedence = 20, .fixity = Operator_Fixity_Postfix }
  });
  scope_define(program->global_scope, slice_literal("."), (Scope_Entry) {
    .type = Scope_Entry_Type_Operator,
    .Operator = { .precedence = 19, .fixity = Operator_Fixity_Infix }
  });
  scope_define(program->global_scope, slice_literal("->"), (Scope_Entry) {
    .type = Scope_Entry_Type_Operator,
    .Operator = { .precedence = 19, .fixity = Operator_Fixity_Infix }
  });
  scope_define(program->global_scope, slice_literal("macro"), (Scope_Entry) {
    .type = Scope_Entry_Type_Operator,
    .Operator = { .precedence = 19, .fixity = Operator_Fixity_Prefix }
  });
  scope_define(program->global_scope, slice_literal("@"), (Scope_Entry) {
    .type = Scope_Entry_Type_Operator,
    .Operator = { .precedence = 18, .fixity = Operator_Fixity_Prefix }
  });

  scope_define(program->global_scope, slice_literal("-"), (Scope_Entry) {
    .type = Scope_Entry_Type_Operator,
    .Operator = {
      .precedence = 17,
      .handler = token_handle_negation,
      .argument_count = 1,
      .fixity = Operator_Fixity_Prefix
    }
  });

  scope_define(program->global_scope, slice_literal("&"), (Scope_Entry) {
    .type = Scope_Entry_Type_Operator,
    .Operator = { .precedence = 16, .fixity = Operator_Fixity_Prefix }
  });
  scope_define(program->global_scope, slice_literal("*"), (Scope_Entry) {
    .type = Scope_Entry_Type_Operator,
    .Operator = { .precedence = 15, .fixity = Operator_Fixity_Infix }
  });
  scope_define(program->global_scope, slice_literal("/"), (Scope_Entry) {
    .type = Scope_Entry_Type_Operator,
    .Operator = { .precedence = 15, .fixity = Operator_Fixity_Infix }
  });
  scope_define(program->global_scope, slice_literal("%"), (Scope_Entry) {
    .type = Scope_Entry_Type_Operator,
    .Operator = { .precedence = 15, .fixity = Operator_Fixity_Infix }
  });

  scope_define(program->global_scope, slice_literal("+"), (Scope_Entry) {
    .type = Scope_Entry_Type_Operator,
    .Operator = { .precedence = 10, .fixity = Operator_Fixity_Infix }
  });
  scope_define(program->global_scope, slice_literal("-"), (Scope_Entry) {
    .type = Scope_Entry_Type_Operator,
    .Operator = { .precedence = 10, .fixity = Operator_Fixity_Infix }
  });


  scope_define(program->global_scope, slice_literal("<"), (Scope_Entry) {
    .type = Scope_Entry_Type_Operator,
    .Operator = { .precedence = 8, .fixity = Operator_Fixity_Infix }
  });
  scope_define(program->global_scope, slice_literal(">"), (Scope_Entry) {
    .type = Scope_Entry_Type_Operator,
    .Operator = { .precedence = 8, .fixity = Operator_Fixity_Infix }
  });
  scope_define(program->global_scope, slice_literal("<="), (Scope_Entry) {
    .type = Scope_Entry_Type_Operator,
    .Operator = { .precedence = 8, .fixity = Operator_Fixity_Infix }
  });
  scope_define(program->global_scope, slice_literal(">="), (Scope_Entry) {
    .type = Scope_Entry_Type_Operator,
    .Operator = { .precedence = 8, .fixity = Operator_Fixity_Infix }
  });

  scope_define(program->global_scope, slice_literal("=="), (Scope_Entry) {
    .type = Scope_Entry_Type_Operator,
    .Operator = { .precedence = 7, .fixity = Operator_Fixity_Infix }
  });
  scope_define(program->global_scope, slice_literal("!="), (Scope_Entry) {
    .type = Scope_Entry_Type_Operator,
    .Operator = { .precedence = 7, .fixity = Operator_Fixity_Infix }
  });


  scope_define(program->global_scope, slice_literal("&&"), (Scope_Entry) {
    .type = Scope_Entry_Type_Operator,
    .Operator = { .precedence = 5, .fixity = Operator_Fixity_Infix }
  });
  scope_define(program->global_scope, slice_literal("||"), (Scope_Entry) {
    .type = Scope_Entry_Type_Operator,
    .Operator = { .precedence = 4, .fixity = Operator_Fixity_Infix }
  });


  scope_define(program->global_scope, slice_literal("any"), (Scope_Entry) {
    .type = Scope_Entry_Type_Value,
    .value = type_any_value
  });

  scope_define(program->global_scope, slice_literal("Register_8"), (Scope_Entry) {
    .type = Scope_Entry_Type_Value,
    .value = type_register_8_value
  });
  scope_define(program->global_scope, slice_literal("Register_16"), (Scope_Entry) {
    .type = Scope_Entry_Type_Value,
    .value = type_register_16_value
  });
  scope_define(program->global_scope, slice_literal("Register_32"), (Scope_Entry) {
    .type = Scope_Entry_Type_Value,
    .value = type_register_32_value
  });
  scope_define(program->global_scope, slice_literal("Register_64"), (Scope_Entry) {
    .type = Scope_Entry_Type_Value,
    .value = type_register_64_value
  });

  #define MASS_PROCESS_BUILT_IN_TYPE(_NAME_, _BIT_SIZE_)\
    scope_define(program->global_scope, slice_literal(#_NAME_), (Scope_Entry) {\
      .type = Scope_Entry_Type_Value,\
      .value = type_##_NAME_##_value\
    });
  MASS_ENUMERATE_BUILT_IN_TYPES
  #undef MASS_PROCESS_BUILT_IN_TYPE

  {
    Array_Token_Statement_Matcher matchers =
      dyn_array_make(Array_Token_Statement_Matcher, .allocator = allocator);
    dyn_array_push(matchers, (Token_Statement_Matcher){token_parse_statement_label});
    dyn_array_push(matchers, (Token_Statement_Matcher){token_parse_statement_if});
    dyn_array_push(matchers, (Token_Statement_Matcher){token_parse_inline_machine_code_bytes});
    dyn_array_push(matchers, (Token_Statement_Matcher){token_parse_assignment});
    dyn_array_push(matchers, (Token_Statement_Matcher){token_parse_definition_and_assignment_statements});
    dyn_array_push(matchers, (Token_Statement_Matcher){token_parse_definitions});
    dyn_array_push(matchers, (Token_Statement_Matcher){token_parse_explicit_return});
    dyn_array_push(matchers, (Token_Statement_Matcher){token_parse_goto});
    dyn_array_push(matchers, (Token_Statement_Matcher){token_parse_constant_definitions});
    program->global_scope->statement_matchers = matchers;
  }

  return program;
};

void
program_deinit(
  Program *program
) {
  for (u64 i = 0; i < dyn_array_length(program->import_libraries); ++i) {
    Import_Library *library = dyn_array_get(program->import_libraries, i);
    dyn_array_destroy(library->symbols);
  }
  bucket_buffer_destroy(program->data_section.buffer);
  bucket_buffer_destroy(program->code_section.buffer);
  dyn_array_destroy(program->labels);
  dyn_array_destroy(program->patch_info_array);
  dyn_array_destroy(program->import_libraries);
  dyn_array_destroy(program->functions);
  if (program->jit_buffer) {
    fixed_buffer_destroy(program->jit_buffer);
  }
}

void
compilation_context_init(
  const Allocator *allocator,
  Compilation_Context *context
) {
  Bucket_Buffer *compilation_buffer = bucket_buffer_make(.allocator = allocator_system);
  Allocator *compilation_allocator = bucket_buffer_allocator_make(compilation_buffer);
  Program *program = allocator_allocate(compilation_allocator, Program);
  program_init(compilation_allocator, program);
  *context = (Compilation_Context) {
    .allocation_buffer = compilation_buffer,
    .allocator = compilation_allocator,
    .program = program,
    .scope = program->global_scope,
    .result = allocator_allocate(compilation_allocator, Mass_Result)
  };
}

void
compilation_context_deinit(
  Compilation_Context *context
) {
  program_deinit(context->program);
  bucket_buffer_destroy(context->allocation_buffer);
}

void
program_set_label_offset(
  Program *program,
  Label_Index label_index,
  u32 offset_in_section
) {
  Label *label = program_get_label(program, label_index);
  label->offset_in_section = offset_in_section;
}

static inline u32
program_resolve_label_to_rva(
  const Program *program,
  const Label *label
) {
  return label->section->base_rva + label->offset_in_section;
}

void
program_patch_labels(
  Program *program
) {
  for (
    u64 patch_index = 0;
    patch_index < dyn_array_length(program->patch_info_array);
    ++patch_index
  ) {
    Label_Location_Diff_Patch_Info *info = dyn_array_get(program->patch_info_array, patch_index);
    Label *target_label = program_get_label(program, info->target_label_index);

    s64 from_rva = program_resolve_label_to_rva(program, &info->from);
    s64 target_rva = program_resolve_label_to_rva(program, target_label);

    s64 diff = target_rva - from_rva;
    *info->patch_target = s64_to_s32(diff);
  }
}

void
program_jit(
  Compilation_Context *context
) {
  #ifdef _WIN32
  win32_program_jit(context);
  #else
  panic("JIT compilation is (yet) not implemented for this system");
  #endif
}

Import_Library *
program_find_import_library(
  const Program *program,
  const Slice library_name
) {
  for (u64 i = 0; i < dyn_array_length(program->import_libraries); ++i) {
    Import_Library *lib = dyn_array_get(program->import_libraries, i);
    if (slice_ascii_case_insensitive_equal(lib->name, library_name)) {
      return lib;
    }
  }
  return 0;
}

Import_Symbol *
import_library_find_symbol(
  const Import_Library *library,
  const Slice symbol_name
) {
  for (u64 i = 0; i < dyn_array_length(library->symbols); ++i) {
    Import_Symbol *symbol = dyn_array_get(library->symbols, i);
    if (slice_equal(symbol->name, symbol_name)) {
      return symbol;
    }
  }
  return 0;
}

Import_Symbol *
program_find_import(
  const Program *program,
  const Slice library_name,
  const Slice symbol_name
) {
  Import_Library *lib = program_find_import_library(program, library_name);
  if (!lib) return 0;
  return import_library_find_symbol(lib, symbol_name);
}

Operand
import_symbol(
  Compilation_Context *context,
  const Slice library_name,
  const Slice symbol_name
) {
  Program *program = context->program;
  Import_Library *library = program_find_import_library(program, library_name);
  if (!library) {
    library = dyn_array_push(program->import_libraries, (Import_Library) {
      .name = library_name,
      .name_rva = 0xCCCCCCCC,
      .rva = 0xCCCCCCCC,
      .image_thunk_rva = 0xCCCCCCCC,
      .symbols = dyn_array_make(Array_Import_Symbol, .allocator = context->allocator),
    });
  }

  Import_Symbol *symbol = import_library_find_symbol(library, symbol_name);

  if (!symbol) {
    // FIXME move to a readonly section
    Label_Index label = make_label(program, &program->data_section);
    symbol = dyn_array_push(library->symbols, (Import_Symbol) {
      .name = symbol_name,
      .name_rva = 0xCCCCCCCC,
      .label32 = label,
    });
  }

  u32 byte_size = 8;
  return data_label32(symbol->label32, byte_size);
}

Value *
c_function_import(
  Compilation_Context *context,
  const char *library_name,
  const char *forward_declaration
) {
  char *symbol_name_end = strchr(forward_declaration, '(');
  assert(symbol_name_end);
  char *symbol_name_start = symbol_name_end;
  while (symbol_name_start != forward_declaration && !isspace(*symbol_name_start)) {
    --symbol_name_start;
  }
  ++symbol_name_start;
  u64 length = symbol_name_end - symbol_name_start;
  char *symbol_name = allocator_allocate_array(context->allocator, char, length + 1);
  memcpy(symbol_name, symbol_name_start, length);
  symbol_name[length] = 0;

  Value *result = allocator_allocate(context->allocator, Value);
  *result = (const Value) {
    .descriptor = c_function_descriptor(context->allocator, forward_declaration),
    .operand = import_symbol(
      context,
      slice_from_c_string(library_name),
      slice_from_c_string(symbol_name)
    ),
  };
  return result;
}

#define FUNCTION_PROLOG_EPILOG_MAX_INSTRUCTION_COUNT 16

u64
estimate_max_code_size_in_bytes(
  Program *program
) {
  u64 total_instruction_count = 0;
  for (u64 i = 0; i < dyn_array_length(program->functions); ++i) {
    Function_Builder *builder = dyn_array_get(program->functions, i);
    total_instruction_count += dyn_array_length(builder->code_block.instructions);
    total_instruction_count += FUNCTION_PROLOG_EPILOG_MAX_INSTRUCTION_COUNT;
  }
  // TODO this should be architecture-dependent
  const u64 max_bytes_per_instruction = 15;
  return total_instruction_count * max_bytes_per_instruction;
}

inline bool
same_value_type(
  Value *a,
  Value *b
) {
  return same_type(a->descriptor, b->descriptor);
}

bool
same_value_type_or_can_implicitly_move_cast(
  Value *target,
  Value *source
) {
  if (same_value_type(target, source)) return true;
  // Allow literal `0` to be cast to a pointer
  if (
    target->descriptor->tag == Descriptor_Tag_Pointer &&
    descriptor_is_integer(source->descriptor) &&
    source->operand.tag == Operand_Tag_Immediate &&
    operand_immediate_value_up_to_s64(&source->operand) == 0
  ) {
    return true;
  }
  if (target->descriptor->tag != source->descriptor->tag) return false;
  // TODO deal with signess
  if (descriptor_is_integer(source->descriptor) && descriptor_is_integer(target->descriptor)) {
    if (descriptor_byte_size(target->descriptor) > descriptor_byte_size(source->descriptor)) {
      return true;
    }
    if (source->operand.tag == Operand_Tag_Immediate) {

      #define ACCEPT_IF_INTEGER_IMMEDIATE_FITS(_SOURCE_TYPE_, _TARGET_TYPE_)\
        if (source->descriptor == &descriptor_##_SOURCE_TYPE_) {\
          assert(target->descriptor == &descriptor_##_TARGET_TYPE_);\
          return _SOURCE_TYPE_##_fits_into_##_TARGET_TYPE_(\
            operand_immediate_memory_as_##_SOURCE_TYPE_(&source->operand)\
          );\
        }

      if (descriptor_is_signed_integer(target->descriptor)) {
        ACCEPT_IF_INTEGER_IMMEDIATE_FITS(u8, s8)
        ACCEPT_IF_INTEGER_IMMEDIATE_FITS(u16, s16)
        ACCEPT_IF_INTEGER_IMMEDIATE_FITS(u32, s32)
        ACCEPT_IF_INTEGER_IMMEDIATE_FITS(u64, s64)
      } else {
        assert(descriptor_is_unsigned_integer(target->descriptor));
        ACCEPT_IF_INTEGER_IMMEDIATE_FITS(s8, u8)
        ACCEPT_IF_INTEGER_IMMEDIATE_FITS(s16, u16)
        ACCEPT_IF_INTEGER_IMMEDIATE_FITS(s32, u32)
        ACCEPT_IF_INTEGER_IMMEDIATE_FITS(s64, u64)
      }

      #undef ACCEPT_IF_INTEGER_IMMEDIATE_FITS
    }
  }
  return false;
}


