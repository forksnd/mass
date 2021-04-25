#ifndef C_MACRO_H
#define C_MACRO_H
#include <stddef.h>

#define COMPILER_SOURCE_LOCATION_GLOBAL_FIELDS\
  {\
    .filename = __FILE__,\
    .function_name = "GLOBAL",\
    .line_number = __LINE__,\
  }

#define COMPILER_SOURCE_LOCATION_FIELDS\
  {\
    .filename = __FILE__,\
    .function_name = __func__,\
    .line_number = __LINE__,\
  }
#define COMPILER_SOURCE_LOCATION\
  ((Compiler_Source_Location)COMPILER_SOURCE_LOCATION_FIELDS)

#define MASS_DEFINE_POINTER_DESCRIPTOR(_NAME_)\
  static Descriptor descriptor_##_NAME_##_pointer = {\
    .tag = Descriptor_Tag_Pointer_To,\
    .name = slice_literal_fields(#_NAME_),\
    .bit_size = (sizeof(void *) * CHAR_BIT),\
    .bit_alignment = _Alignof(void *) * CHAR_BIT,\
    .Pointer_To.descriptor = &descriptor_##_NAME_,\
  }

#define MASS_DEFINE_OPAQUE_DESCRIPTOR(_NAME_, _BIT_SIZE_, _BIT_ALIGNMENT_)\
  static Descriptor descriptor_##_NAME_ = {\
    .tag = Descriptor_Tag_Opaque,\
    .name = slice_literal_fields(#_NAME_),\
    .bit_size = (_BIT_SIZE_),\
    .bit_alignment = (_BIT_ALIGNMENT_),\
  };\
  MASS_DEFINE_POINTER_DESCRIPTOR(_NAME_);\
  MASS_DEFINE_POINTER_DESCRIPTOR(_NAME_##_pointer)

#define MASS_DEFINE_STRUCT_DESCRIPTOR(_NAME_, _C_TYPE_, ...)\
  dyn_array_struct(Memory_Layout_Item) descriptor_##_NAME_##_fields = {\
    .length = countof((const Memory_Layout_Item[]){__VA_ARGS__}),\
    .items = {__VA_ARGS__},\
  };\
  static Descriptor descriptor_##_NAME_ = {\
    .tag = Descriptor_Tag_Struct,\
    .name = slice_literal_fields(#_NAME_),\
    .bit_size = (sizeof(_C_TYPE_) * CHAR_BIT),\
    .bit_alignment = _Alignof(_C_TYPE_) * CHAR_BIT,\
    .Struct = {\
      .memory_layout = {\
        .items = {(Dyn_Array_Internal *)&descriptor_##_NAME_##_fields},\
      }\
    },\
  };\
  MASS_DEFINE_POINTER_DESCRIPTOR(_NAME_);\
  MASS_DEFINE_POINTER_DESCRIPTOR(_NAME_##_pointer)

#define VALUE_STATIC_EPOCH 0

#define MASS_TYPE_VALUE(_DESCRIPTOR_)\
  (Value) {\
    .epoch = VALUE_STATIC_EPOCH,\
    .descriptor = &descriptor_type,\
    .storage = {\
      .tag = Storage_Tag_Static,\
      .byte_size = sizeof(Descriptor),\
      .Static.memory = {.tag = Static_Memory_Tag_Heap, .Heap.pointer = (_DESCRIPTOR_)},\
    },\
    .compiler_source_location = COMPILER_SOURCE_LOCATION_GLOBAL_FIELDS,\
  }

#define MASS_DEFINE_TYPE_VALUE(_NAME_)\
  static Value *type_##_NAME_##_value = &MASS_TYPE_VALUE(&descriptor_##_NAME_)

#define MASS_DEFINE_OPAQUE_TYPE(_NAME_, _BIT_SIZE_, _BIT_ALIGNMENT_)\
  MASS_DEFINE_OPAQUE_DESCRIPTOR(_NAME_, _BIT_SIZE_, _BIT_ALIGNMENT_);\
  MASS_DEFINE_TYPE_VALUE(_NAME_);

#define MASS_DEFINE_OPAQUE_C_TYPE(_NAME_, _C_TYPE_)\
  MASS_DEFINE_OPAQUE_TYPE(_NAME_, sizeof(_C_TYPE_) * CHAR_BIT, _Alignof(_C_TYPE_) * CHAR_BIT)

#define MASS_ENUMERATE_INTEGER_TYPES\
  MASS_PROCESS_BUILT_IN_TYPE(s8, 8, 8)\
  MASS_PROCESS_BUILT_IN_TYPE(s16, 16, 16)\
  MASS_PROCESS_BUILT_IN_TYPE(s32, 32, 32)\
  MASS_PROCESS_BUILT_IN_TYPE(s64, 64, 64)\
  MASS_PROCESS_BUILT_IN_TYPE(u8, 8, 8)\
  MASS_PROCESS_BUILT_IN_TYPE(u16, 16, 16)\
  MASS_PROCESS_BUILT_IN_TYPE(u32, 32, 32)\
  MASS_PROCESS_BUILT_IN_TYPE(u64, 64, 64)

#define MASS_ENUMERATE_BUILT_IN_TYPES\
  MASS_ENUMERATE_INTEGER_TYPES \
  MASS_PROCESS_BUILT_IN_TYPE(f32, 32, 32)\
  MASS_PROCESS_BUILT_IN_TYPE(f64, 64, 64)

typedef struct {
  Slice name;
  s32 value;
} C_Enum_Item;

#include "generated_types.h"

static Descriptor descriptor_void = {
  .tag = Descriptor_Tag_Void,
  .name = slice_literal_fields("void"),
};
MASS_DEFINE_POINTER_DESCRIPTOR(void);

#endif // C_MACRO_H
