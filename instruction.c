#include "value.h"

typedef enum {
  Instruction_Extension_Type_None,
  Instruction_Extension_Type_Register,
  Instruction_Extension_Type_Op_Code,
  Instruction_Extension_Type_Plus_Register,
} Instruction_Extension_Type;

typedef enum {
  Operand_Encoding_Type_None,
  Operand_Encoding_Type_Op_Code_Plus_Register,
  Operand_Encoding_Type_Register,
  Operand_Encoding_Type_Register_Memory,
  Operand_Encoding_Type_Immediate_8,
  Operand_Encoding_Type_Immediate_32,
  Operand_Encoding_Type_Immediate_64,
} Operand_Encoding_Type;

typedef struct {
  u8 op_code[2];
  Instruction_Extension_Type extension_type;
  u8 op_code_extension;
  Operand_Encoding_Type operand_encoding_types[3];
} Instruction_Encoding;

typedef struct {
  const Instruction_Encoding *encoding_list;
  u32 encoding_count;
} X64_Mnemonic;

typedef struct {
  const X64_Mnemonic mnemonic;
  Operand operands[3];
} Instruction;

////////////////////////////////////////////////////////////////////////////////
// mov
////////////////////////////////////////////////////////////////////////////////
const Instruction_Encoding mov_encoding_list[] = {
  {
    .op_code = { 0x00, 0x89 },
    .extension_type = Instruction_Extension_Type_Register,
    .operand_encoding_types = {
      Operand_Encoding_Type_Register_Memory,
      Operand_Encoding_Type_Register,
      Operand_Encoding_Type_None
    },
  },
  {
    .op_code = { 0x00, 0x8B },
    .extension_type = Instruction_Extension_Type_Register,
    .operand_encoding_types = {
      Operand_Encoding_Type_Register,
      Operand_Encoding_Type_Register_Memory,
      Operand_Encoding_Type_None
    },
  },
  {
    .op_code = { 0x00, 0xc7 },
    .extension_type = Instruction_Extension_Type_Op_Code,
    .op_code_extension = 0,
    .operand_encoding_types = {
      Operand_Encoding_Type_Register_Memory,
      Operand_Encoding_Type_Immediate_32,
      Operand_Encoding_Type_None
    },
  },
  {
    .op_code = { 0x00, 0xb8 },
    .extension_type = Instruction_Extension_Type_None,
    .operand_encoding_types = {
      Operand_Encoding_Type_Op_Code_Plus_Register,
      Operand_Encoding_Type_Immediate_64,
      Operand_Encoding_Type_None
    },
  },
};

const X64_Mnemonic mov = {
  .encoding_list = (const Instruction_Encoding *)mov_encoding_list,
  .encoding_count = static_array_size(mov_encoding_list),
};

////////////////////////////////////////////////////////////////////////////////
// ret
////////////////////////////////////////////////////////////////////////////////
const Instruction_Encoding ret_encoding_list[] = {
  {
    .op_code = { 0x00, 0xc3 },
    .extension_type = Instruction_Extension_Type_Register,
    .operand_encoding_types = {
      Operand_Encoding_Type_None,
      Operand_Encoding_Type_None,
      Operand_Encoding_Type_None
    },
  },
};
const X64_Mnemonic ret = {
  .encoding_list = (const Instruction_Encoding *)ret_encoding_list,
  .encoding_count = static_array_size(ret_encoding_list),
};

////////////////////////////////////////////////////////////////////////////////
// add
////////////////////////////////////////////////////////////////////////////////
const Instruction_Encoding add_encoding_list[] = {
  {
    .op_code = { 0x00, 0x03 },
    .extension_type = Instruction_Extension_Type_Register,
    .operand_encoding_types = {
      Operand_Encoding_Type_Register,
      Operand_Encoding_Type_Register_Memory,
      Operand_Encoding_Type_None
    },
  },
  {
    .op_code = { 0x00, 0x83 },
    .extension_type = Instruction_Extension_Type_Op_Code,
    .op_code_extension = 0,
    .operand_encoding_types = {
      Operand_Encoding_Type_Register_Memory,
      Operand_Encoding_Type_Immediate_8,
      Operand_Encoding_Type_None
    },
  },
};
const X64_Mnemonic add = {
  .encoding_list = (const Instruction_Encoding *)add_encoding_list,
  .encoding_count = static_array_size(add_encoding_list),
};

////////////////////////////////////////////////////////////////////////////////
// sub
////////////////////////////////////////////////////////////////////////////////
const Instruction_Encoding sub_encoding_list[] = {
  {
    .op_code = { 0x00, 0x29 },
    .extension_type = Instruction_Extension_Type_Register,
    .operand_encoding_types = {
      Operand_Encoding_Type_Register_Memory,
      Operand_Encoding_Type_Register,
      Operand_Encoding_Type_None
    },
  },
  {
    .op_code = { 0x00, 0x2B },
    .extension_type = Instruction_Extension_Type_Register,
    .operand_encoding_types = {
      Operand_Encoding_Type_Register,
      Operand_Encoding_Type_Register_Memory,
      Operand_Encoding_Type_None
    },
  },
  {
    .op_code = { 0x00, 0x83 },
    .extension_type = Instruction_Extension_Type_Op_Code,
    .op_code_extension = 5,
    .operand_encoding_types = {
      Operand_Encoding_Type_Register_Memory,
      Operand_Encoding_Type_Immediate_8,
      Operand_Encoding_Type_None
    },
  },
};
const X64_Mnemonic sub = {
  .encoding_list = (const Instruction_Encoding *)sub_encoding_list,
  .encoding_count = static_array_size(sub_encoding_list),
};

////////////////////////////////////////////////////////////////////////////////
// imul
////////////////////////////////////////////////////////////////////////////////
const Instruction_Encoding imul_encoding_list[] = {
  {
    .op_code = { 0x00, 0x69 },
    .extension_type = Instruction_Extension_Type_Register,
    .operand_encoding_types = {
      Operand_Encoding_Type_Register,
      Operand_Encoding_Type_Register_Memory,
      Operand_Encoding_Type_Immediate_32
    },
  },
};
const X64_Mnemonic imul = {
  .encoding_list = (const Instruction_Encoding *)imul_encoding_list,
  .encoding_count = static_array_size(imul_encoding_list),
};

////////////////////////////////////////////////////////////////////////////////
// idiv
////////////////////////////////////////////////////////////////////////////////
const Instruction_Encoding idiv_encoding_list[] = {
  {
    .op_code = { 0x00, 0xF7 },
    .extension_type = Instruction_Extension_Type_Op_Code,
    .op_code_extension = 7,
    .operand_encoding_types = {
      Operand_Encoding_Type_Register_Memory
    },
  },
};
const X64_Mnemonic idiv = {
  .encoding_list = (const Instruction_Encoding *)idiv_encoding_list,
  .encoding_count = static_array_size(idiv_encoding_list),
};


////////////////////////////////////////////////////////////////////////////////
// cwd/cdq/cqo
////////////////////////////////////////////////////////////////////////////////
const Instruction_Encoding cqo_encoding_list[] = {
  {
    .op_code = { 0x00, 0x99 },
    .extension_type = Instruction_Extension_Type_Op_Code,
    .operand_encoding_types = {
      Operand_Encoding_Type_None,
      Operand_Encoding_Type_None,
      Operand_Encoding_Type_None
    },
  },
};
const X64_Mnemonic cqo = {
  .encoding_list = (const Instruction_Encoding *)cqo_encoding_list,
  .encoding_count = static_array_size(cqo_encoding_list),
};
const X64_Mnemonic cdq = {
  .encoding_list = (const Instruction_Encoding *)cqo_encoding_list,
  .encoding_count = static_array_size(cqo_encoding_list),
};
const X64_Mnemonic cwd = {
  .encoding_list = (const Instruction_Encoding *)cqo_encoding_list,
  .encoding_count = static_array_size(cqo_encoding_list),
};


////////////////////////////////////////////////////////////////////////////////
// call
////////////////////////////////////////////////////////////////////////////////
const Instruction_Encoding call_encoding_list[] = {
  {
    .op_code = { 0x00, 0xFF },
    .extension_type = Instruction_Extension_Type_Op_Code,
    .op_code_extension = 2,
    .operand_encoding_types = {
      Operand_Encoding_Type_Register_Memory,
      Operand_Encoding_Type_None,
      Operand_Encoding_Type_None
    },
  },
};
const X64_Mnemonic call = {
  .encoding_list = (const Instruction_Encoding *)call_encoding_list,
  .encoding_count = static_array_size(call_encoding_list),
};


////////////////////////////////////////////////////////////////////////////////
// cmp
////////////////////////////////////////////////////////////////////////////////
const Instruction_Encoding cmp_encoding_list[] = {
  {
    .op_code = { 0x00, 0x81 },
    .extension_type = Instruction_Extension_Type_Op_Code,
    .op_code_extension = 7,
    .operand_encoding_types = {
      Operand_Encoding_Type_Register_Memory,
      Operand_Encoding_Type_Immediate_32,
      Operand_Encoding_Type_None
    },
  },
};
const X64_Mnemonic cmp = {
  .encoding_list = (const Instruction_Encoding *)cmp_encoding_list,
  .encoding_count = static_array_size(cmp_encoding_list),
};


////////////////////////////////////////////////////////////////////////////////
// jnz
////////////////////////////////////////////////////////////////////////////////
const Instruction_Encoding jnz_encoding_list[] = {
  {
    .op_code = { 0x00, 0x75 },
    .extension_type = Instruction_Extension_Type_None,
    .operand_encoding_types = {
      Operand_Encoding_Type_Immediate_8,
      Operand_Encoding_Type_None,
      Operand_Encoding_Type_None
    },
  },
  {
    .op_code = { 0x0F, 0x85 },
    .extension_type = Instruction_Extension_Type_None,
    .operand_encoding_types = {
      Operand_Encoding_Type_Immediate_32,
      Operand_Encoding_Type_None,
      Operand_Encoding_Type_None
    },
  },
};
const X64_Mnemonic jnz = {
  .encoding_list = (const Instruction_Encoding *)jnz_encoding_list,
  .encoding_count = static_array_size(jnz_encoding_list),
};


////////////////////////////////////////////////////////////////////////////////
// jmp
////////////////////////////////////////////////////////////////////////////////
const Instruction_Encoding jmp_encoding_list[] = {
  {
    .op_code = { 0x00, 0xEB },
    .extension_type = Instruction_Extension_Type_None,
    .operand_encoding_types = {
      Operand_Encoding_Type_Immediate_8,
      Operand_Encoding_Type_None,
      Operand_Encoding_Type_None
    },
  },
  {
    .op_code = { 0x00, 0xE9 },
    .extension_type = Instruction_Extension_Type_None,
    .operand_encoding_types = {
      Operand_Encoding_Type_Immediate_32,
      Operand_Encoding_Type_None,
      Operand_Encoding_Type_None
    },
  },
};
const X64_Mnemonic jmp = {
  .encoding_list = (const Instruction_Encoding *)jmp_encoding_list,
  .encoding_count = static_array_size(jmp_encoding_list),
};
