/** @file
  Try to load an EFI-stubbed ARM Linux kernel from QEMU's fw_cfg.

  This implementation differs from OvmfPkg/Library/LoadLinuxLib. An EFI
  stub in the subject kernel is a hard requirement here.

  Copyright (C) 2014-2016, Red Hat, Inc.

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/DevicePathLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/PeCoffLib.h>
#include <Library/QemuFwCfgLib.h>
#include <Library/ReportStatusCodeLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>

#include <Protocol/DevicePath.h>
#include <Protocol/LoadedImage.h>
#include <Protocol/LoadFile.h>
#include <Protocol/LoadFile2.h>

#pragma pack(1)
typedef struct {
  VENDOR_DEVICE_PATH          VenMediaNode;
  EFI_DEVICE_PATH_PROTOCOL    EndNode;
} SINGLE_NODE_VENDOR_MEDIA_DEVPATH;
#pragma pack()

STATIC CONST SINGLE_NODE_VENDOR_MEDIA_DEVPATH mKernelDevicePath = {
  {
    {
      MEDIA_DEVICE_PATH, MEDIA_VENDOR_DP, { sizeof (VENDOR_DEVICE_PATH) }
    },
    {
      0xb0fae7e7, 0x6b07, 0x49d0,
      { 0x9e, 0x5b, 0x3b, 0xde, 0xc8, 0x3b, 0x03, 0x9d }
    }
  },

  {
    END_DEVICE_PATH_TYPE, END_ENTIRE_DEVICE_PATH_SUBTYPE,
    { sizeof (EFI_DEVICE_PATH_PROTOCOL) }
  }
};

STATIC CONST SINGLE_NODE_VENDOR_MEDIA_DEVPATH mInitrdDevicePath = {
  {
    {
      MEDIA_DEVICE_PATH, MEDIA_VENDOR_DP, { sizeof (VENDOR_DEVICE_PATH) }
    },
    {
      // LINUX_EFI_INITRD_MEDIA_GUID
      0x5568e427, 0x68fc, 0x4f3d,
      { 0xac, 0x74, 0xca, 0x55, 0x52, 0x31, 0xcc, 0x68 }
    }
  },

  {
    END_DEVICE_PATH_TYPE, END_ENTIRE_DEVICE_PATH_SUBTYPE,
    { sizeof (EFI_DEVICE_PATH_PROTOCOL) }
  }
};

STATIC
EFI_STATUS
EFIAPI
KernelLoadFile (
  IN EFI_LOAD_FILE_PROTOCOL           *This,
  IN EFI_DEVICE_PATH_PROTOCOL         *FilePath,
  IN BOOLEAN                          BootPolicy,
  IN OUT UINTN                        *BufferSize,
  IN VOID                             *Buffer OPTIONAL
  )
{
  UINTN                     SetupSize;
  UINTN                     KernelSize;

  if (BufferSize == NULL || !IsDevicePathValid (FilePath, 0)) {
    return EFI_INVALID_PARAMETER;
  }

  QemuFwCfgSelectItem (QemuFwCfgItemKernelSetupSize);
  SetupSize = (UINTN)QemuFwCfgRead64 ();

  QemuFwCfgSelectItem (QemuFwCfgItemKernelSize);
  KernelSize = (UINTN)QemuFwCfgRead64 ();

  if (Buffer == NULL || *BufferSize < SetupSize + KernelSize) {
    *BufferSize = SetupSize + KernelSize;
    return EFI_BUFFER_TOO_SMALL;
  }

  QemuFwCfgSelectItem (QemuFwCfgItemKernelSetupData);
  QemuFwCfgReadBytes (SetupSize, Buffer);

  QemuFwCfgSelectItem (QemuFwCfgItemKernelData);
  QemuFwCfgReadBytes (KernelSize, (UINT8 *)Buffer + SetupSize);

  *BufferSize = SetupSize + KernelSize;
  return EFI_SUCCESS;
}

STATIC CONST EFI_LOAD_FILE_PROTOCOL     mKernelLoadFile = {
  KernelLoadFile,
};

STATIC
EFI_STATUS
EFIAPI
InitrdLoadFile2 (
  IN EFI_LOAD_FILE2_PROTOCOL          *This,
  IN EFI_DEVICE_PATH_PROTOCOL         *FilePath,
  IN BOOLEAN                          BootPolicy,
  IN OUT UINTN                        *BufferSize,
  IN VOID                             *Buffer OPTIONAL
  )
{
  UINTN                     InitrdSize;

  if (BootPolicy) {
    return EFI_UNSUPPORTED;
  }

  if (BufferSize == NULL || !IsDevicePathValid (FilePath, 0)) {
    return EFI_INVALID_PARAMETER;
  }

  if (FilePath->Type != END_DEVICE_PATH_TYPE ||
      FilePath->SubType != END_ENTIRE_DEVICE_PATH_SUBTYPE) {
    return EFI_NOT_FOUND;
  }

  QemuFwCfgSelectItem (QemuFwCfgItemInitrdSize);
  InitrdSize = (UINTN)QemuFwCfgRead64 ();

  if (Buffer == NULL || *BufferSize < InitrdSize) {
    *BufferSize = InitrdSize;
    return EFI_BUFFER_TOO_SMALL;
  }

  QemuFwCfgSelectItem (QemuFwCfgItemInitrdData);
  QemuFwCfgReadBytes (InitrdSize, Buffer);

  // TODO measure initrd image into the appropriate PCR

  *BufferSize = InitrdSize;
  return EFI_SUCCESS;
}

STATIC CONST EFI_LOAD_FILE2_PROTOCOL     mInitrdLoadFile2 = {
  InitrdLoadFile2,
};

//
// The entry point of the feature.
//

/**
  Download the kernel, the initial ramdisk, and the kernel command line from
  QEMU's fw_cfg. Construct a minimal SimpleFileSystem that contains the two
  image files, and load and start the kernel from it.

  The kernel will be instructed via its command line to load the initrd from
  the same Simple FileSystem.

  @retval EFI_NOT_FOUND         Kernel image was not found.
  @retval EFI_OUT_OF_RESOURCES  Memory allocation failed.
  @retval EFI_PROTOCOL_ERROR    Unterminated kernel command line.

  @return                       Error codes from any of the underlying
                                functions. On success, the function doesn't
                                return.
**/
EFI_STATUS
EFIAPI
TryRunningQemuKernel (
  VOID
  )
{
  EFI_HANDLE                KernelHandle;
  EFI_HANDLE                InitrdHandle;
  EFI_HANDLE                KernelImageHandle;
  EFI_LOADED_IMAGE_PROTOCOL *KernelLoadedImage;
  EFI_STATUS                Status;
  CHAR8                     *CommandLine;
  UINTN                     CommandLineSize;
  UINTN                     InitrdSize;
  UINTN                     Idx;

  KernelHandle = NULL;
  Status = gBS->InstallMultipleProtocolInterfaces (&KernelHandle,
                  &gEfiDevicePathProtocolGuid,    &mKernelDevicePath,
                  &gEfiLoadFileProtocolGuid,      &mKernelLoadFile,
                  NULL);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: InstallMultipleProtocolInterfaces(): %r\n",
      __FUNCTION__, Status));
    return Status;
  }

  QemuFwCfgSelectItem (QemuFwCfgItemInitrdSize);
  InitrdSize = (UINTN)QemuFwCfgRead64 ();

  if (InitrdSize > 0) {
    InitrdHandle = NULL;
    Status = gBS->InstallMultipleProtocolInterfaces (&InitrdHandle,
                    &gEfiDevicePathProtocolGuid,    &mInitrdDevicePath,
                    &gEfiLoadFile2ProtocolGuid,     &mInitrdLoadFile2,
                    NULL);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: InstallMultipleProtocolInterfaces(): %r\n",
        __FUNCTION__, Status));
      goto UninstallKernelHandle;
    }
  }

  //
  // Load the image. This should call back into our loadfile protocol.
  //
  Status = gBS->LoadImage (
                  FALSE,             // BootPolicy: exact match required
                  gImageHandle,      // ParentImageHandle
                  (EFI_DEVICE_PATH_PROTOCOL *)&mKernelDevicePath,
                  NULL,              // SourceBuffer
                  0,                 // SourceSize
                  &KernelImageHandle
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: LoadImage(): %r\n", __FUNCTION__, Status));
    if (Status != EFI_SECURITY_VIOLATION) {
      goto UninstallInitrdHandle;
    }
    //
    // From the resource allocation perspective, EFI_SECURITY_VIOLATION means
    // "success", so we must roll back the image loading.
    //
    goto UnloadKernelImage;
  }

  //
  // Construct the kernel command line.
  //
  Status = gBS->OpenProtocol (
                  KernelImageHandle,
                  &gEfiLoadedImageProtocolGuid,
                  (VOID **)&KernelLoadedImage,
                  gImageHandle,                  // AgentHandle
                  NULL,                          // ControllerHandle
                  EFI_OPEN_PROTOCOL_GET_PROTOCOL
                  );
  ASSERT_EFI_ERROR (Status);

  QemuFwCfgSelectItem (QemuFwCfgItemCommandLineSize);
  CommandLineSize = (UINTN)QemuFwCfgRead64 ();

  if (CommandLineSize > 0) {
    KernelLoadedImage->LoadOptionsSize = CommandLineSize * sizeof(CHAR16);
    KernelLoadedImage->LoadOptions = AllocatePool (
                                       KernelLoadedImage->LoadOptionsSize);
    if (KernelLoadedImage->LoadOptions == NULL) {
      KernelLoadedImage->LoadOptionsSize = 0;
      Status = EFI_OUT_OF_RESOURCES;
      goto UnloadKernelImage;
    }

    CommandLine = (CHAR8 *)KernelLoadedImage->LoadOptions + CommandLineSize;

    QemuFwCfgSelectItem (QemuFwCfgItemCommandLineData);
    QemuFwCfgReadBytes (CommandLineSize, CommandLine);

    //
    // Verify NUL-termination of the command line.
    //
    if (CommandLine[CommandLineSize - 1] != '\0') {
      DEBUG ((DEBUG_ERROR, "%a: kernel command line is not NUL-terminated\n",
        __FUNCTION__));
      Status = EFI_PROTOCOL_ERROR;
      goto UnloadKernelImage;
    }

    for (Idx = 0; Idx < CommandLineSize; Idx++) {
      ((CHAR16 *)KernelLoadedImage->LoadOptions)[Idx] = (CHAR16)CommandLine[Idx];
    }

    DEBUG ((DEBUG_INFO, "%a: command line: \"%s\"\n", __FUNCTION__,
      (CHAR16 *)KernelLoadedImage->LoadOptions));
  }

  //
  // Signal the EFI_EVENT_GROUP_READY_TO_BOOT event.
  //
  EfiSignalEventReadyToBoot();

  REPORT_STATUS_CODE (EFI_PROGRESS_CODE,
    (EFI_SOFTWARE_DXE_BS_DRIVER | EFI_SW_DXE_BS_PC_READY_TO_BOOT_EVENT));

  //
  // Start the image.
  //
  Status = gBS->StartImage (KernelImageHandle, NULL, NULL);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: StartImage(): %r\n", __FUNCTION__, Status));
  }

  if (KernelLoadedImage->LoadOptions != NULL) {
    FreePool (KernelLoadedImage->LoadOptions);
  }
  KernelLoadedImage->LoadOptionsSize = 0;

UnloadKernelImage:
  gBS->UnloadImage (KernelImageHandle);

UninstallInitrdHandle:
  if (InitrdSize > 0) {
    gBS->UninstallMultipleProtocolInterfaces (InitrdHandle,
           &gEfiDevicePathProtocolGuid,    &mInitrdDevicePath,
           &gEfiLoadFile2ProtocolGuid,     &mInitrdLoadFile2,
           NULL);
  }

UninstallKernelHandle:
  gBS->UninstallMultipleProtocolInterfaces (KernelHandle,
         &gEfiDevicePathProtocolGuid,    &mKernelDevicePath,
         &gEfiLoadFileProtocolGuid,      &mKernelLoadFile,
         NULL);

  return Status;
}
