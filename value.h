#ifndef VALUE_H
#define VALUE_H
#include "prelude.h"
#include <inttypes.h>
#include "types.h"
#include "encoding.h"

static Array_Value_Ptr empty_value_array = {
  .internal = &(Dyn_Array_Internal){.allocator = &allocator_static},
};

static inline bool
register_is_xmm(
  Register reg
) {
  return !!(reg & Register_Xmm0);
}

static inline bool
storage_equal(
  const Storage *a,
  const Storage *b
);

#define storage_none ((Storage){.tag = Storage_Tag_None })

static inline void
register_bitset_set(
  u64 *bitset,
  Register reg
);

static inline void
register_bitset_unset(
  u64 *bitset,
  Register reg
);

static inline bool
register_bitset_get(
  u64 bitset,
  Register reg
);

#define MASS_ON_ERROR(...)\
  if ((__VA_ARGS__).tag != Mass_Result_Tag_Success)

#define MASS_TRY(...)\
  for (Mass_Result _result = (__VA_ARGS__); _result.tag != Mass_Result_Tag_Success;) return _result;

PRELUDE_NO_DISCARD static inline Mass_Result
MASS_SUCCESS() {
  return (Mass_Result){.tag = Mass_Result_Tag_Success};
}

static Fixed_Buffer *
mass_error_to_string(
  Mass_Error const* error
);

static inline Value
type_value_for_descriptor(
  Descriptor *descriptor
) {
  return MASS_TYPE_VALUE(descriptor);
}

Value void_value = {
  .descriptor = &descriptor_void,
  .storage = { .tag = Storage_Tag_None },
  .compiler_source_location = COMPILER_SOURCE_LOCATION_GLOBAL_FIELDS,
};

static inline bool
descriptor_is_unsigned_integer(
  const Descriptor *descriptor
) {
  return (
    descriptor == &descriptor_u8  ||
    descriptor == &descriptor_u16 ||
    descriptor == &descriptor_u32 ||
    descriptor == &descriptor_u64
  );
}

static inline bool
descriptor_is_signed_integer(
  const Descriptor *descriptor
) {
  return (
    descriptor == &descriptor_s8  ||
    descriptor == &descriptor_s16 ||
    descriptor == &descriptor_s32 ||
    descriptor == &descriptor_s64
  );
}

static inline bool
descriptor_is_integer(
  const Descriptor *descriptor
) {
  return descriptor_is_signed_integer(descriptor) || descriptor_is_unsigned_integer(descriptor);
}

bool
descriptor_is_float(
  const Descriptor *descriptor
) {
  return descriptor == &descriptor_f32 || descriptor == &descriptor_f64;
}

static inline const Descriptor *
maybe_unwrap_pointer_descriptor(
  const Descriptor *descriptor
) {
  if (descriptor->tag == Descriptor_Tag_Pointer_To) {
    return descriptor->Pointer_To.descriptor;
  }
  return descriptor;
}

static u64
descriptor_bit_size(
  const Descriptor *descriptor
);

static inline u64
descriptor_byte_size(
  const Descriptor *descriptor
);

static inline Descriptor *
descriptor_array_of(
  const Allocator *allocator,
  const Descriptor *item_descriptor,
  u32 length
);

bool
same_type(
  const Descriptor *a,
  const Descriptor *b
);

bool
same_value_type(
  Value *a,
  Value *b
);

void
source_range_print_start_position(
  const Source_Range *source_range
);

Slice
source_from_source_range(
  const Source_Range *source_range
);

#define INSTRUCTION_BYTES_NO_LABEL 255

typedef struct {
  Label_Index end_label;
  Array_Instruction instructions;
  u64 register_volatile_bitset;
  u64 register_occupied_bitset;
  Value *register_occupied_values[Register__Max];
} Code_Block;
static_assert(Register__Min == 0, "Should be psosible to use register indexes as array indexes");

typedef struct Function_Builder {
  bool frozen;
  s32 stack_reserve;
  u32 max_call_parameters_stack_size;
  Code_Block code_block;
  u64 used_register_bitset;
  Slice source;

  const Function_Info *function;
  Label_Index label_index;
} Function_Builder;
typedef dyn_array_type(Function_Builder) Array_Function_Builder;

MASS_DEFINE_OPAQUE_C_TYPE(function_builder, Function_Builder);

typedef struct Program {
  Array_Import_Library import_libraries;
  Array_Label labels;
  Array_Label_Location_Diff_Patch_Info patch_info_array;
  Array_Value_Ptr startup_functions;
  Array_Relocation relocations;
  Value *entry_point;
  Array_Function_Builder functions;
  Program_Memory memory;
} Program;
MASS_DEFINE_OPAQUE_C_TYPE(program, Program);

hash_map_slice_template(Jit_Import_Library_Handle_Map, void *)
hash_map_slice_template(Imported_Module_Map, Module *)

typedef struct Jit {
  bool is_stack_unwinding_in_progress;
  Program *program;
  Jit_Import_Library_Handle_Map *import_library_handles;
  void *platform_specific_payload;
} Jit;

bool
pointer_equal(
  const void * const *a,
  const void * const *b
) {
  return *a == *b;
}
hash_map_template(Static_Pointer_Map, const void *, Value, hash_pointer, pointer_equal)

typedef struct Compilation {
  Virtual_Memory_Buffer allocation_buffer;
  Allocator *allocator;
  Jit jit;
  Module compiler_module;
  Static_Pointer_Map *static_pointer_map;
  Imported_Module_Map *module_map;
  Scope *root_scope;
  Program *runtime_program;
  Mass_Result *result;
} Compilation;

MASS_DEFINE_OPAQUE_C_TYPE(compilation, Compilation);

void *
rip_value_pointer(
  Program *program,
  Value *value
);

static inline bool
storage_is_label(
  const Storage *operand
);

static inline bool
storage_is_register_index(
  const Storage *storage,
  Register reg_index
);

fn_type_opaque
value_as_function(
  const Jit *jit,
  Value *value
);

static inline void *
rip_value_pointer_from_label_index(
  Program *program,
  Label_Index label_index
);

void
print_operand(
  const Storage *operand
);

void
program_jit(
  Jit *jit
);

static inline Label *
program_get_label(
  Program *program,
  Label_Index label
);

void
program_patch_labels(
  Program *program
);

void
program_set_label_offset(
  Program *program,
  Label_Index label_index,
  u32 offset_in_section
);

#endif
