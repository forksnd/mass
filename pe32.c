#include "prelude.h"
#include "assert.h"

typedef struct {
  const char *name;
  u32 name_rva;
  u32 ptr_rva;
} Import_Name_To_Rva;

typedef struct {
  Import_Name_To_Rva dll;
  Import_Name_To_Rva *functions;
  u32 iat_rva;
  u32 image_thunk_rva;
  s32 function_count;
} Import_Library;

void write_executable() {
  Buffer exe_buffer = make_buffer(1024 * 1024, PAGE_READWRITE);
  IMAGE_DOS_HEADER *dos_header = buffer_allocate(&exe_buffer, IMAGE_DOS_HEADER);

  *dos_header = (IMAGE_DOS_HEADER) {
    .e_magic = IMAGE_DOS_SIGNATURE,
    .e_lfanew = sizeof(IMAGE_DOS_HEADER),
  };
  buffer_append_s32(&exe_buffer, IMAGE_NT_SIGNATURE);

  IMAGE_FILE_HEADER *file_header = buffer_allocate(&exe_buffer, IMAGE_FILE_HEADER);

  *file_header = (IMAGE_FILE_HEADER) {
    .Machine = IMAGE_FILE_MACHINE_AMD64,
    .NumberOfSections = 2,
    .TimeDateStamp = 0x5EF48E56, // FIXME generate ourselves
    .SizeOfOptionalHeader = sizeof(IMAGE_OPTIONAL_HEADER64),
    .Characteristics = IMAGE_FILE_EXECUTABLE_IMAGE | IMAGE_FILE_LARGE_ADDRESS_AWARE,
  };

  IMAGE_OPTIONAL_HEADER64 *optional_header = buffer_allocate(&exe_buffer, IMAGE_OPTIONAL_HEADER64);

  enum {
    EXPORT_DIRECTORY_INDEX,
    IMPORT_DIRECTORY_INDEX,
    RESOURCE_DIRECTORY_INDEX,
    EXCEPTION_DIRECTORY_INDEX,
    SECURITY_DIRECTORY_INDEX,
    RELOCATION_DIRECTORY_INDEX,
    DEBUG_DIRECTORY_INDEX,
    ARCHITECTURE_DIRECTORY_INDEX,
    GLOBAL_PTR_DIRECTORY_INDEX,
    TLS_DIRECTORY_INDEX,
    LOAD_CONFIG_DIRECTORY_INDEX,
    BOUND_IMPORT_DIRECTORY_INDEX,
    IAT_DIRECTORY_INDEX,
    DELAY_IMPORT_DIRECTORY_INDEX,
    CLR_DIRECTORY_INDEX,
  };

  *optional_header = (IMAGE_OPTIONAL_HEADER64) {
    .Magic = IMAGE_NT_OPTIONAL_HDR64_MAGIC,
    .SizeOfCode = 0x200, // FIXME calculate based on the amount of machine code
    .SizeOfInitializedData = 0x200, // FIXME calculate based on the amount of global data
    .AddressOfEntryPoint = 0x2000, // FIXME resolve to the entry point in the machine code
    .BaseOfCode = 0x2000, // FIXME resolve to the right section containing code
    .ImageBase = 0x0000000140000000, // Does not matter as we are using dynamic base
    .SectionAlignment = 0x1000,
    .FileAlignment = 0x200,
    .MajorOperatingSystemVersion = 6, // FIXME figure out if can be not hard coded
    .MinorOperatingSystemVersion = 0,
    .MajorSubsystemVersion = 6, // FIXME figure out if can be not hard coded
    .MinorSubsystemVersion = 0,
    .SizeOfImage = 0x3000, // FIXME calculate based on the sizes of the sections
    .SizeOfHeaders = 0,
    .Subsystem = IMAGE_SUBSYSTEM_WINDOWS_CUI, // TODO allow user to specify this
    .DllCharacteristics =
      IMAGE_DLLCHARACTERISTICS_HIGH_ENTROPY_VA |
      IMAGE_DLLCHARACTERISTICS_NX_COMPAT | // TODO figure out what NX is
      IMAGE_DLLCHARACTERISTICS_DYNAMIC_BASE |
      IMAGE_DLLCHARACTERISTICS_TERMINAL_SERVER_AWARE,
    .SizeOfStackReserve = 0x100000,
    .SizeOfStackCommit = 0x1000,
    .SizeOfHeapReserve = 0x100000,
    .SizeOfHeapCommit = 0x1000,
    .NumberOfRvaAndSizes = IMAGE_NUMBEROF_DIRECTORY_ENTRIES,
    .DataDirectory = {0},
  };

  // .rdata section
  IMAGE_SECTION_HEADER *rdata_section_header = buffer_allocate(&exe_buffer, IMAGE_SECTION_HEADER);
  *rdata_section_header = (IMAGE_SECTION_HEADER) {
    .Name = ".rdata",
    .Misc = 0x14C, // FIXME size of machine code in bytes
    .VirtualAddress = 0x1000, // FIXME calculate this
    .SizeOfRawData = 0x200, // FIXME calculate this
    .PointerToRawData = 0,
    .Characteristics = IMAGE_SCN_CNT_INITIALIZED_DATA | IMAGE_SCN_MEM_READ,
  };

  // .text section
  IMAGE_SECTION_HEADER *text_section_header = buffer_allocate(&exe_buffer, IMAGE_SECTION_HEADER);
  *text_section_header = (IMAGE_SECTION_HEADER) {
    .Name = ".text",
    .Misc = 0x10, // FIXME size of machine code in bytes
    .VirtualAddress = optional_header->BaseOfCode,
    .SizeOfRawData = optional_header->SizeOfCode,
    .PointerToRawData = 0,
    .Characteristics = IMAGE_SCN_CNT_CODE | IMAGE_SCN_MEM_READ | IMAGE_SCN_MEM_EXECUTE,
  };

  // NULL header telling that the list is done
  *buffer_allocate(&exe_buffer, IMAGE_SECTION_HEADER) = (IMAGE_SECTION_HEADER){0};

  optional_header->SizeOfHeaders = align((s32)exe_buffer.occupied, optional_header->FileAlignment);
  exe_buffer.occupied = optional_header->SizeOfHeaders;

  IMAGE_SECTION_HEADER *sections[] = {
    rdata_section_header,
    text_section_header,
  };
  s32 section_offset = optional_header->SizeOfHeaders;
  for (u32 i = 0; i < static_array_size(sections); ++i) {
    sections[i]->PointerToRawData = section_offset;
    section_offset += sections[i]->SizeOfRawData;
  }

  #define file_offset_to_rva(_section_header_)\
    ((s32)exe_buffer.occupied - \
     (_section_header_)->PointerToRawData + \
     (_section_header_)->VirtualAddress)


  // .rdata segment
  Import_Name_To_Rva kernel32_functions[] = {
    // @MachineCodePatch
    {.name = "ExitProcess", .name_rva = 0xCCCCCCCC, .ptr_rva = 0xCCCCCCCC},

    {.name = "GetStdHandle", .name_rva = 0xCCCCCCCC, .ptr_rva = 0xCCCCCCCC},
    {.name = "ReadConsoleA", .name_rva = 0xCCCCCCCC, .ptr_rva = 0xCCCCCCCC},
  };

  Import_Name_To_Rva user32_functions[] = {
    {.name = "ShowWindow", .name_rva = 0xCCCCCCCC, .ptr_rva = 0xCCCCCCCC},
  };

  Import_Library import_libraries[] = {
    {
      .dll = {.name = "kernel32.dll", .name_rva = 0xCCCCCCCC},
      .iat_rva = 0xCCCCCCCC,
      .image_thunk_rva = 0xCCCCCCCC,
      .functions = kernel32_functions,
      .function_count = static_array_size(kernel32_functions),
    },
    {
      .dll = {.name = "user32.dll", .name_rva = 0xCCCCCCCC},
      .iat_rva = 0xCCCCCCCC,
      .image_thunk_rva = 0xCCCCCCCC,
      .functions = user32_functions,
      .function_count = static_array_size(user32_functions),
    },
  };


  // IAT
  exe_buffer.occupied = rdata_section_header->PointerToRawData;

  for (u64 i = 0; i < static_array_size(import_libraries); ++i) {
    Import_Library *lib = &import_libraries[i];
    for (s32 i = 0; i < lib->function_count; ++i) {
      lib->functions[i].name_rva = file_offset_to_rva(rdata_section_header);
      buffer_append_s16(&exe_buffer, 0); // Ordinal Hint, value not required
      size_t name_size = strlen(lib->functions[i].name) + 1;
      s32 aligned_name_size = align((s32)name_size, 2);
      memcpy(
        buffer_allocate_size(&exe_buffer, aligned_name_size),
        lib->functions[i].name,
        name_size
      );
    }
  }

  optional_header->DataDirectory[IAT_DIRECTORY_INDEX].VirtualAddress =
    file_offset_to_rva(rdata_section_header);
  for (u64 i = 0; i < static_array_size(import_libraries); ++i) {
    Import_Library *lib = &import_libraries[i];
    lib->iat_rva = file_offset_to_rva(rdata_section_header);
    for (s32 i = 0; i < lib->function_count; ++i) {
      Import_Name_To_Rva *fn = &lib->functions[i];
      fn->ptr_rva = file_offset_to_rva(rdata_section_header);
      buffer_append_u64(&exe_buffer, fn->name_rva);
    }
    // End of IAT list
    buffer_append_u64(&exe_buffer, 0);
  }
  optional_header->DataDirectory[IAT_DIRECTORY_INDEX].Size =
    (s32)(exe_buffer.occupied - rdata_section_header->PointerToRawData);


  // Image thunks
  for (u64 i = 0; i < static_array_size(import_libraries); ++i) {
    Import_Library *lib = &import_libraries[i];
    lib->image_thunk_rva = file_offset_to_rva(rdata_section_header);

    for (s32 i = 0; i < lib->function_count; ++i) {
      buffer_append_u64(&exe_buffer, lib->functions[i].name_rva);
    }
    // End of IAT list
    buffer_append_u64(&exe_buffer, 0);
  }

  buffer_append_s64(&exe_buffer, 0);

  // Library Names

  for (u64 i = 0; i < static_array_size(import_libraries); ++i) {
    Import_Library *lib = &import_libraries[i];
    lib->dll.name_rva = file_offset_to_rva(rdata_section_header);
    size_t name_size = strlen(lib->dll.name) + 1;
    s32 aligned_name_size = align((s32)name_size, 2);
    memcpy(
      buffer_allocate_size(&exe_buffer, aligned_name_size),
      lib->dll.name,
      name_size
    );
  }

  // Import Directory
  s32 import_directory_rva = file_offset_to_rva(rdata_section_header);
  optional_header->DataDirectory[IMPORT_DIRECTORY_INDEX].VirtualAddress = import_directory_rva;

  for (u64 i = 0; i < static_array_size(import_libraries); ++i) {
    Import_Library *lib = &import_libraries[i];

    IMAGE_IMPORT_DESCRIPTOR *image_import_descriptor =
      buffer_allocate(&exe_buffer, IMAGE_IMPORT_DESCRIPTOR);
    *image_import_descriptor = (IMAGE_IMPORT_DESCRIPTOR) {
      .OriginalFirstThunk = lib->image_thunk_rva,
      .Name = lib->dll.name_rva,
      .FirstThunk = lib->iat_rva,
    };
  }

  optional_header->DataDirectory[IMPORT_DIRECTORY_INDEX].Size =
    file_offset_to_rva(rdata_section_header) - import_directory_rva;

  // End of IMAGE_IMPORT_DESCRIPTOR list
  *buffer_allocate(&exe_buffer, IMAGE_IMPORT_DESCRIPTOR) = (IMAGE_IMPORT_DESCRIPTOR) {0};

  exe_buffer.occupied =
    rdata_section_header->PointerToRawData + rdata_section_header->SizeOfRawData;

  // .text segment
  exe_buffer.occupied = text_section_header->PointerToRawData;

  buffer_append_s8(&exe_buffer, 0x48); // sub rsp 28
  buffer_append_s8(&exe_buffer, 0x83);
  buffer_append_s8(&exe_buffer, 0xEC);
  buffer_append_s8(&exe_buffer, 0x28);

  buffer_append_s8(&exe_buffer, 0xB9); // mov rcx, 42
  buffer_append_s8(&exe_buffer, 0x2A);
  buffer_append_s8(&exe_buffer, 0x00);
  buffer_append_s8(&exe_buffer, 0x00);
  buffer_append_s8(&exe_buffer, 0x00);

  buffer_append_s8(&exe_buffer, 0xFF); // call ExitProcess
  buffer_append_s8(&exe_buffer, 0x15);
  {
    s32 *ExitProcess_rip_relative_address = buffer_allocate(&exe_buffer, s32);
    s32 ExitProcess_call_rva = file_offset_to_rva(text_section_header);

    bool match_found = false;
    for (u64 i = 0; !match_found && i < static_array_size(import_libraries); ++i) {
      Import_Library *lib = &import_libraries[i];
      if (strcmp(lib->dll.name, "kernel32.dll") != 0) continue;

      for (s32 i = 0; i < lib->function_count; ++i) {
        Import_Name_To_Rva *fn = &lib->functions[i];
        if (strcmp(fn->name, "ExitProcess") == 0) {
          *ExitProcess_rip_relative_address = fn->ptr_rva - ExitProcess_call_rva;
          match_found = true;
          break;
        }
      }
    }
    assert(match_found);
  }

  buffer_append_s8(&exe_buffer, 0xCC); // int3

  exe_buffer.occupied =
    text_section_header->PointerToRawData + text_section_header->SizeOfRawData;

  /////////

  HANDLE file = CreateFile(
    L"build\\test.exe",    // name of the write
    GENERIC_WRITE,         // open for writing
    0,                     // do not share
    0,                     // default security
    CREATE_ALWAYS,         // create new file only
    FILE_ATTRIBUTE_NORMAL, // normal file
    0                      // no attr. template
  );

  assert(file != INVALID_HANDLE_VALUE);

  DWORD bytes_written = 0;
  WriteFile(
    file,                 // open file handle
    exe_buffer.memory,    // start of data to write
    (DWORD) exe_buffer.occupied,  // number of bytes to write
    &bytes_written,       // number of bytes that were written
    0
  );

  CloseHandle(file);
}
