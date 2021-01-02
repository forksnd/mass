#include "value.h"
#include "encoding.h"

typedef enum {
  MOD_Displacement_0   = 0b00,
  MOD_Displacement_s8  = 0b01,
  MOD_Displacement_s32 = 0b10,
  MOD_Register         = 0b11,
} MOD;

typedef enum {
  REX   = 0b01000000,
  REX_W = 0b01001000, // 0 = Operand size determined by CS.D; 1 = 64 Bit Operand Size
  REX_R = 0b01000100, // Extension of the ModR/M reg field
  REX_X = 0b01000010, // Extension of the SIB index field
  REX_B = 0b01000001, // Extension of the ModR/M r/m field, SIB base field, or Opcode reg field
} REX_BYTE;

void
encode_instruction_assembly(
  Program *program,
  Fixed_Buffer *buffer,
  Instruction *instruction,
  const Instruction_Encoding *encoding,
  u32 operand_count
) {
  assert(instruction->type == Instruction_Type_Assembly);
  u64 original_buffer_length = buffer->occupied;

  s8 mod_r_m_operand_index = -1;
  u8 reg_or_op_code = 0;
  u8 rex_byte = 0;
  bool needs_16_bit_prefix = false;
  u8 r_m = 0;
  u8 mod = MOD_Register;
  u8 op_code[4] = {
    encoding->op_code[0],
    encoding->op_code[1],
    encoding->op_code[2],
    encoding->op_code[3],
  };
  bool needs_sib = false;
  u8 sib_byte = 0;
  s32 displacement = 0;

  for (u8 operand_index = 0; operand_index < operand_count; ++operand_index) {
    Operand *operand = &instruction->assembly.operands[operand_index];
    const Operand_Encoding *operand_encoding = &encoding->operands[operand_index];

    if (operand->byte_size == 2) {
      needs_16_bit_prefix = true;
    }

    if (
      operand->byte_size == 8 &&
      !(
        operand_encoding->type == Operand_Encoding_Type_Xmm ||
        operand_encoding->type == Operand_Encoding_Type_Xmm_Memory
      )
    ) {
      rex_byte |= REX_W;
    }

    if (operand->tag == Operand_Tag_Register) {
      if (operand_encoding->type == Operand_Encoding_Type_Register) {
        if (encoding->extension_type == Instruction_Extension_Type_Plus_Register) {
          op_code[3] += operand->Register.index & 0b111;
          if (operand->Register.index & 0b1000) {
            rex_byte |= REX_B;
          }
        } else {
          assert(encoding->extension_type != Instruction_Extension_Type_Op_Code);
          reg_or_op_code = operand->Register.index;
          if (operand->Register.index & 0b1000) {
            rex_byte |= REX_R;
          }
        }
      }
    }

    if (
      operand->tag == Operand_Tag_Xmm &&
      operand_encoding->type == Operand_Encoding_Type_Xmm &&
      encoding->extension_type == Instruction_Extension_Type_Register
    ) {
      reg_or_op_code = operand->Register.index;
    }

    if(
      operand_encoding->type == Operand_Encoding_Type_Memory ||
      operand_encoding->type == Operand_Encoding_Type_Register_Memory ||
      operand_encoding->type == Operand_Encoding_Type_Xmm_Memory
    ) {
      if (mod_r_m_operand_index != -1) {
        panic("Multiple MOD R/M operands are not supported in an instruction");
      }
      mod_r_m_operand_index = operand_index;
      if (operand->tag == Operand_Tag_Register) {
        r_m = operand->Register.index;
        mod = MOD_Register;
      } else if (operand->tag == Operand_Tag_Xmm) {
        r_m = operand->Register.index;
        mod = MOD_Register;
      } else if (operand->tag == Operand_Tag_Memory) {
        const Memory_Location *location = &operand->Memory.location;
        switch(location->tag) {
          case Memory_Location_Tag_Instruction_Pointer_Relative: {
            r_m = 0b101;
            break;
          }
          case Memory_Location_Tag_Indirect: {
            // Right now the compiler does not support SIB scale other than 1.
            // From what I can tell there are two reasons that only matter in *extremely*
            // performance-sensitive code which probably would be written by hand anyway:
            // 1) `add rax, 8` is three bytes longer than `inc rax`. With current instruction
            //    cache sizes it is very unlikely to be problematic.
            // 2) Loop uses the same index for arrays of values of different sizes that could
            //    be represented with SIB scale. In cases of extreme register pressure this
            //    can cause spilling. To avoid that we could try to use temporary shifts
            //    to adjust the offset between different indexes, but it is not implemented ATM.
            enum { SIB_Scale_1 = 0b00, };
            u8 sib_scale_bits = SIB_Scale_1;
            if (operand->Memory.location.Indirect.index.tag == Memory_Indirect_Operand_Tag_Immediate) {
              assert(operand->Memory.location.Indirect.index.Immediate.value == 0);
            }
            assert(operand->Memory.location.Indirect.base.tag == Memory_Indirect_Operand_Tag_Register);
            Register base = operand->Memory.location.Indirect.base.Register.index;
            // [RSP + X] always needs to be encoded as SIB because RSP register index
            // in MOD R/M is occupied by RIP-relative encoding
            if (base == Register_SP) {
              needs_sib = true;
              r_m = 0b0100; // SIB
              sib_byte = (
                ((sib_scale_bits & 0b11) << 6) |
                ((Register_SP & 0b111) << 3) |
                ((Register_SP & 0b111) << 0)
              );
            } else if (
              operand->Memory.location.Indirect.index.tag == Memory_Indirect_Operand_Tag_Register
            ) {
              needs_sib = true;
              r_m = 0b0100; // SIB
              Register sib_index = operand->Memory.location.Indirect.index.Register.index;
              sib_byte = (
                ((sib_scale_bits & 0b11) << 6) |
                ((sib_index & 0b111) << 3) |
                ((base & 0b111) << 0)
              );
              if (sib_index & 0b1000) {
                rex_byte |= REX_X;
              }
            } else {
              r_m = operand->Memory.location.Indirect.base.Register.index;
            }
            displacement = s64_to_s32(operand->Memory.location.Indirect.offset);
            break;
          }
        }
        if (displacement == 0) {
          mod = MOD_Displacement_0;
        } else if (s32_fits_into_s8(displacement)) {
          mod = MOD_Displacement_s8;
        } else {
          mod = MOD_Displacement_s32;
        }
      } else {
        panic("Unsupported operand type");
      }
    }
  }

  if (encoding->extension_type == Instruction_Extension_Type_Op_Code) {
    reg_or_op_code = encoding->op_code_extension;
  }

  if (r_m & 0b1000) {
    rex_byte |= REX_B;
  }

  if (rex_byte) {
    fixed_buffer_append_u8(buffer, rex_byte);
  }

  if (needs_16_bit_prefix) {
    fixed_buffer_append_u8(buffer, 0x66);
  }

  if (op_code[0]) {
    fixed_buffer_append_u8(buffer, op_code[0]);
  }
  if (op_code[1]) {
    fixed_buffer_append_u8(buffer, op_code[1]);
  }
  if (op_code[2]) {
    fixed_buffer_append_u8(buffer, op_code[2]);
  }
  fixed_buffer_append_u8(buffer, op_code[3]);

  if (mod_r_m_operand_index != -1) {
    u8 mod_r_m = (
      (mod << 6) |
      ((reg_or_op_code & 0b111) << 3) |
      ((r_m & 0b111))
    );
    fixed_buffer_append_u8(buffer, mod_r_m);
  }

  if (needs_sib) {
    fixed_buffer_append_u8(buffer, sib_byte);
  }

  // :AfterInstructionPatch
  // Some operands are encoded as diff from a certain machine code byte
  // to the byte *after* the end of the instruction being encoded.
  // These operands can appear before the immediates, which means that
  // we do not know what the diff should be before the immediate is
  // encoded as well. To solve this we store the patch locations and do
  // a loop over them after the instruction has been encoded.
  Label_Location_Diff_Patch_Info *mod_r_m_patch_info = 0;
  Label_Location_Diff_Patch_Info *immediate_label_patch_info = 0;

  // Write out displacement
  if (mod_r_m_operand_index != -1 && mod != MOD_Register) {
    const Operand *operand = &instruction->assembly.operands[mod_r_m_operand_index];
    assert (operand->tag == Operand_Tag_Memory);
    const Memory_Location *location = &operand->Memory.location;
    switch(location->tag) {
      case Memory_Location_Tag_Instruction_Pointer_Relative: {
        Label_Index label_index =
          operand->Memory.location.Instruction_Pointer_Relative.label_index;
        // :OperandNormalization
        s32 *patch_target = fixed_buffer_allocate_unaligned(buffer, s32);
        // :AfterInstructionPatch
        mod_r_m_patch_info =
          dyn_array_push(program->patch_info_array, (Label_Location_Diff_Patch_Info) {
            .target_label_index = label_index,
            .from = {.section = &program->code_section},
            .patch_target = patch_target,
          });
        break;
      }
      case Memory_Location_Tag_Indirect: {
        if (mod == MOD_Displacement_s32) {
          fixed_buffer_append_s32(buffer, displacement);
        } else if (mod == MOD_Displacement_s8) {
          fixed_buffer_append_s8(buffer, s32_to_s8(displacement));
        } else {
          assert(mod == MOD_Displacement_0);
        }
        break;
      }
    }
  }

  // Write out immediate operand(s?)
  for (u32 operand_index = 0; operand_index < operand_count; ++operand_index) {
    const Operand_Encoding *operand_encoding = &encoding->operands[operand_index];
    if (operand_encoding->type != Operand_Encoding_Type_Immediate) {
      continue;
    }
    Operand *operand = &instruction->assembly.operands[operand_index];
    if (operand_is_label(operand)) {
      Label_Index label_index = operand->Memory.location.Instruction_Pointer_Relative.label_index;
      s32 *patch_target = fixed_buffer_allocate_unaligned(buffer, s32);
      // :AfterInstructionPatch
      immediate_label_patch_info =
        dyn_array_push(program->patch_info_array, (Label_Location_Diff_Patch_Info) {
          .target_label_index = label_index,
          .from = {.section = &program->code_section},
          .patch_target = patch_target,
        });
    } else if (operand->tag == Operand_Tag_Immediate) {
      Slice slice = {
        .bytes = operand->Immediate.memory,
        .length = operand->byte_size,
      };
      fixed_buffer_append_slice(buffer, slice);
    } else {
      panic("Unexpected mismatched operand type for immediate encoding.");
    }
  }

  instruction->encoded_byte_size = u64_to_u8(buffer->occupied - original_buffer_length);

  // :AfterInstructionPatch
  u32 next_instruction_offset = u64_to_u32(buffer->occupied);
  if (mod_r_m_patch_info) {
    mod_r_m_patch_info->from.offset_in_section = next_instruction_offset;
  }
  if (immediate_label_patch_info) {
    immediate_label_patch_info->from.offset_in_section = next_instruction_offset;
  }
}

void
encode_instruction(
  Program *program,
  Fixed_Buffer *buffer,
  Instruction *instruction
) {
  // TODO turn into a switch statement on type
  if (instruction->type == Instruction_Type_Label) {
    Label *label = program_get_label(program, instruction->label);
    label->section = &program->code_section;
    label->offset_in_section = u64_to_u32(buffer->occupied);
    instruction->encoded_byte_size = 0;
    return;
  } else if (instruction->type == Instruction_Type_Bytes) {
    u32 instruction_start_offset = u64_to_u32(buffer->occupied);
    Slice slice = {
      .bytes = (char *)instruction->Bytes.memory,
      .length = instruction->Bytes.length,
    };
    fixed_buffer_append_slice(buffer, slice);

    if (instruction->Bytes.label_offset_in_instruction != INSTRUCTION_BYTES_NO_LABEL) {
      u64 patch_offset_in_buffer =
        instruction_start_offset + instruction->Bytes.label_offset_in_instruction;
      dyn_array_push(program->patch_info_array, (Label_Location_Diff_Patch_Info) {
        .target_label_index = instruction->Bytes.label_index,
        .from = {
          .section = &program->code_section,
          .offset_in_section = u64_to_u32(buffer->occupied),
        },
        .patch_target = (s32 *)(buffer->memory + patch_offset_in_buffer),
      });
    }

    return;
  }

  u32 operand_count = countof(instruction->assembly.operands);
  for (u32 index = 0; index < instruction->assembly.mnemonic->encoding_count; ++index) {
    const Instruction_Encoding *encoding = &instruction->assembly.mnemonic->encoding_list[index];
    for (u32 operand_index = 0; operand_index < operand_count; ++operand_index) {
      const Operand_Encoding *operand_encoding = &encoding->operands[operand_index];
      Operand *operand = &instruction->assembly.operands[operand_index];
      u32 encoding_size = s32_to_u32(operand_encoding->size);

      if (operand_encoding->size != Operand_Size_Any) {
        if (operand->byte_size != encoding_size) {
          encoding = 0;
          break;
        }
      }
      if (
        operand->tag == Operand_Tag_Eflags &&
        operand_encoding->type == Operand_Encoding_Type_Eflags
      ) {
        continue;
      }

      if (
        operand->tag == Operand_Tag_None &&
        operand_encoding->type == Operand_Encoding_Type_None
      ) {
        continue;
      }
      if (
        operand->tag == Operand_Tag_Register &&
        operand->Register.index == Register_A &&
        operand_encoding->type == Operand_Encoding_Type_Register_A
      ) {
        continue;
      }
      if (
        operand->tag == Operand_Tag_Register &&
        operand_encoding->type == Operand_Encoding_Type_Register
      ) {
        continue;
      }
      if (
        operand->tag == Operand_Tag_Register &&
        operand_encoding->type == Operand_Encoding_Type_Register_Memory
      ) {
        continue;
      }
      if (
        operand->tag == Operand_Tag_Memory &&
        operand_encoding->type == Operand_Encoding_Type_Register_Memory
      ) {
        continue;
      }
      if (
        operand->tag == Operand_Tag_Memory &&
        operand_encoding->type == Operand_Encoding_Type_Memory
      ) {
        continue;
      }
      if (
        operand->tag == Operand_Tag_Xmm &&
        operand_encoding->type == Operand_Encoding_Type_Xmm
      ) {
        continue;
      }
      if (
        operand->tag == Operand_Tag_Xmm &&
        operand_encoding->type == Operand_Encoding_Type_Xmm_Memory
      ) {
        continue;
      }
      if (
        operand->tag == Operand_Tag_Memory &&
        operand_encoding->type == Operand_Encoding_Type_Xmm_Memory
      ) {
        continue;
      }
      if (operand_encoding->type == Operand_Encoding_Type_Immediate) {
        if (operand_is_immediate(operand)) {
          assert(encoding_size == operand->byte_size);
          continue;
        } else if (operand_is_label(operand)) {
          assert(encoding_size == Operand_Size_32);
          continue;
        }
      }
      encoding = 0;
      break;
    }

    if (encoding) {
      encode_instruction_assembly(program, buffer, instruction, encoding, operand_count);
      return;
    }
  }
  const Compiler_Source_Location *compiler_location = &instruction->compiler_source_location;
  if (compiler_location) {
    printf(
      "Added in compiler at %s:%u (fn: %s)\n",
      compiler_location->filename,
      compiler_location->line_number,
      compiler_location->function_name
    );
  } else {
    printf("Unknown compiler location\n");
  }
  const Source_Range *source_range = &instruction->source_range;
  if (source_range) {
    printf("Source code at ");
    source_range_print_start_position(source_range);
  } else {
    printf("Unknown source location\n");
  }
  printf("%s", instruction->assembly.mnemonic->name);
  for (u32 operand_index = 0; operand_index < operand_count; ++operand_index) {
    Operand *operand = &instruction->assembly.operands[operand_index];
    printf(" ");
    print_operand(operand);
  }
  printf("\n");
  assert(!"Did not find acceptable encoding");
}
