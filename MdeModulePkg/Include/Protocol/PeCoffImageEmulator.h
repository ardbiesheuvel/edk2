/** @file
  Copyright (c) 2017, Linaro, Ltd. All rights reserved.<BR>

  This program and the accompanying materials
  are licensed and made available under the terms and conditions of the BSD License
  which accompanies this distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/

#ifndef __PECOFF_IMAGE_EMULATOR_PROTOCOL_GUID_H__
#define __PECOFF_IMAGE_EMULATOR_PROTOCOL_GUID_H__

#define EDKII_PECOFF_IMAGE_EMULATOR_PROTOCOL_GUID \
  { 0x96F46153, 0x97A7, 0x4793, { 0xAC, 0xC1, 0xFA, 0x19, 0xBF, 0x78, 0xEA, 0x97 } }

typedef struct _EDKII_PECOFF_IMAGE_EMULATOR_PROTOCOL EDKII_PECOFF_IMAGE_EMULATOR_PROTOCOL;

typedef
BOOLEAN
(EFIAPI *IS_PECOFF_IMAGE_SUPPORTED) (
  IN  EDKII_PECOFF_IMAGE_EMULATOR_PROTOCOL    *This,
  IN  UINT16                                  MachineType,
  IN  UINT16                                  ImageType
  );

typedef
EFI_STATUS
(EFIAPI *REGISTER_PECOFF_IMAGE) (
  IN  EDKII_PECOFF_IMAGE_EMULATOR_PROTOCOL    *This,
  IN  EFI_PHYSICAL_ADDRESS                    ImageBase,
  IN  UINT64                                  ImageSize
  );

typedef
EFI_STATUS
(EFIAPI *UNREGISTER_PECOFF_IMAGE) (
  IN  EDKII_PECOFF_IMAGE_EMULATOR_PROTOCOL    *This,
  IN  EFI_PHYSICAL_ADDRESS                    ImageBase
  );

typedef struct _EDKII_PECOFF_IMAGE_EMULATOR_PROTOCOL {
  IS_PECOFF_IMAGE_SUPPORTED                   IsImageSupported;
  REGISTER_PECOFF_IMAGE                       RegisterImage;
  UNREGISTER_PECOFF_IMAGE                     UnregisterImage;
} EDKII_PECOFF_IMAGE_EMULATOR_PROTOCOL;

#endif
