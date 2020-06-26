/** @file
Header file for Elf64 convert solution

Copyright (c) 2010 - 2018, Intel Corporation. All rights reserved.<BR>

SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef _ELF_64_CONVERT_
#define _ELF_64_CONVERT_

//
// .note.gnu.property types
//
#define NT_GNU_PROPERTY_TYPE_0                5

#define GNU_PROPERTY_X86_FEATURE_1_AND        0xc0000002
#define GNU_PROPERTY_AARCH64_FEATURE_1_AND    0xc0000000

//
// Bits of GNU_PROPERTY_X86_FEATURE_1_AND
//
#define GNU_PROPERTY_X86_FEATURE_1_IBT        0x00000001
#define GNU_PROPERTY_X86_FEATURE_1_SHSTK      0x00000002

//
// Bits of GNU_PROPERTY_AARCH64_FEATURE_1_AND
//
#define GNU_PROPERTY_AARCH64_FEATURE_1_BTI    0x00000001
#define GNU_PROPERTY_AARCH64_FEATURE_1_PAC    0x00000002

#pragma pack(1)

typedef struct {
  UINT32  Type;
  UINT32  DataSize;
  UINT8   Data[1];
} NT_GNU_PROPERTY;

typedef struct {
  UINT32            NameSize;     // 4 for "GNU"
  UINT32            DescSize;     // size of desc array
  UINT32            Type;         // '5' for NT_GNU_PROPERTY_TYPE_0
  CHAR8             Name[4];      // "GNU"
  NT_GNU_PROPERTY   Properties[1];
} NT_GNU_PROPERTY_TYPE0;

#pragma pack()

BOOLEAN
InitializeElf64 (
  UINT8               *FileBuffer,
  ELF_FUNCTION_TABLE  *ElfFunctions
  );

#endif
