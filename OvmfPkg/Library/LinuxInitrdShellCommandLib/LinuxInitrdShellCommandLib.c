/** @file
  Provides initrd command to load a Linux initrd via its GUIDed vendor media
  path

  Copyright (c) 2020, Arm, Ltd. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <Uefi.h>

#include <Library/DebugLib.h>
#include <Library/DevicePathLib.h>
#include <Library/HiiLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/ShellCommandLib.h>
#include <Library/ShellLib.h>
#include <Library/UefiBootServicesTableLib.h>

#include <Protocol/DevicePath.h>
#include <Protocol/LoadFile2.h>

#pragma pack(1)
typedef struct {
  VENDOR_DEVICE_PATH          VenMediaNode;
  EFI_DEVICE_PATH_PROTOCOL    EndNode;
} SINGLE_NODE_VENDOR_MEDIA_DEVPATH;
#pragma pack()

STATIC CONST CHAR16         mFileName[] = L"<unspecified>";
STATIC EFI_HII_HANDLE       gLinuxInitrdShellCommandHiiHandle;
STATIC SHELL_FILE_HANDLE    mInitrdFileHandle;
STATIC EFI_HANDLE           mInitrdLoadFile2Handle;

STATIC CONST SHELL_PARAM_ITEM ParamList[] = {
  {L"-u", TypeFlag},
  {NULL, TypeMax}
  };

/**
  Get the filename to get help text from if not using HII.

  @retval The filename.
**/
STATIC
CONST CHAR16*
EFIAPI
ShellCommandGetManFileNameInitrd (
  VOID
  )
{
  return mFileName;
}

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
InitrdLoadFile2 (
  IN EFI_LOAD_FILE2_PROTOCOL          *This,
  IN EFI_DEVICE_PATH_PROTOCOL         *FilePath,
  IN BOOLEAN                          BootPolicy,
  IN OUT UINTN                        *BufferSize,
  IN VOID                             *Buffer OPTIONAL
  )
{
  UINT64                    InitrdSize;
  EFI_STATUS                Status;

  if (BootPolicy) {
    return EFI_UNSUPPORTED;
  }

  if (BufferSize == NULL || !IsDevicePathValid (FilePath, 0)) {
    return EFI_INVALID_PARAMETER;
  }

  if (FilePath->Type != END_DEVICE_PATH_TYPE ||
      FilePath->SubType != END_ENTIRE_DEVICE_PATH_SUBTYPE ||
      mInitrdFileHandle == NULL) {
    return EFI_NOT_FOUND;
  }

  Status = gEfiShellProtocol->GetFileSize (mInitrdFileHandle, &InitrdSize);
  ASSERT_EFI_ERROR(Status);

  if (Buffer == NULL || *BufferSize < InitrdSize) {
    *BufferSize = InitrdSize;
    return EFI_BUFFER_TOO_SMALL;
  }

  return gEfiShellProtocol->ReadFile (mInitrdFileHandle, BufferSize, Buffer);
}

STATIC CONST EFI_LOAD_FILE2_PROTOCOL     mInitrdLoadFile2 = {
  InitrdLoadFile2,
};

/**
  Function for 'initrd' command.

  @param[in] ImageHandle  Handle to the Image (NULL if Internal).
  @param[in] SystemTable  Pointer to the System Table (NULL if Internal).
**/
SHELL_STATUS
EFIAPI
ShellCommandRunInitrd (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_STATUS            Status;
  LIST_ENTRY            *Package;
  CHAR16                *ProblemParam;
  CONST CHAR16          *Param;
  CONST CHAR16          *Filename;
  SHELL_STATUS          ShellStatus;

  ProblemParam        = NULL;
  ShellStatus         = SHELL_SUCCESS;

  Status = ShellInitialize ();
  ASSERT_EFI_ERROR (Status);

  //
  // parse the command line
  //
  Status = ShellCommandLineParse (ParamList, &Package, &ProblemParam, TRUE);
  if (EFI_ERROR (Status)) {
    if (Status == EFI_VOLUME_CORRUPTED && ProblemParam != NULL) {
      ShellPrintHiiEx (-1, -1, NULL, STRING_TOKEN (STR_GEN_PROBLEM),
        gLinuxInitrdShellCommandHiiHandle, L"initrd", ProblemParam);
      FreePool (ProblemParam);
      ShellStatus = SHELL_INVALID_PARAMETER;
    } else {
      ASSERT(FALSE);
    }
  } else {
    if (ShellCommandLineGetCount (Package) > 2) {
      ShellPrintHiiEx (-1, -1, NULL, STRING_TOKEN (STR_GEN_TOO_MANY),
        gLinuxInitrdShellCommandHiiHandle, L"initrd");
      ShellStatus = SHELL_INVALID_PARAMETER;
    } else if (ShellCommandLineGetCount (Package) < 2) {
      if (ShellCommandLineGetFlag(Package, L"-u")) {
        if (mInitrdFileHandle != NULL) {
          ShellCloseFile (&mInitrdFileHandle);
          mInitrdFileHandle = NULL;
        }
        if (mInitrdLoadFile2Handle != NULL) {
            gBS->UninstallMultipleProtocolInterfaces (mInitrdLoadFile2Handle,
                   &gEfiDevicePathProtocolGuid,    &mInitrdDevicePath,
                   &gEfiLoadFile2ProtocolGuid,     &mInitrdLoadFile2,
                   NULL);
            mInitrdLoadFile2Handle = NULL;
        }
      } else {
        ShellPrintHiiEx (-1, -1, NULL, STRING_TOKEN (STR_GEN_TOO_FEW),
          gLinuxInitrdShellCommandHiiHandle, L"initrd");
        ShellStatus = SHELL_INVALID_PARAMETER;
      }
    } else {
      Param = ShellCommandLineGetRawValue (Package, 1);
      ASSERT (Param != NULL);

      Filename = ShellFindFilePath (Param);
      if (Filename == NULL) {
        ShellPrintHiiEx (-1, -1, NULL, STRING_TOKEN (STR_GEN_FIND_FAIL),
          gLinuxInitrdShellCommandHiiHandle, L"initrd", Param);
        ShellStatus = SHELL_NOT_FOUND;
      } else {
        if (mInitrdFileHandle != NULL) {
          ShellCloseFile (&mInitrdFileHandle);
          mInitrdFileHandle = NULL;
        }
        Status = ShellOpenFileByName (Filename, &mInitrdFileHandle,
                   EFI_FILE_MODE_READ, 0);
        if (EFI_ERROR (Status)) {
          ShellPrintHiiEx (-1, -1, NULL, STRING_TOKEN (STR_GEN_FILE_OPEN_FAIL),
            gLinuxInitrdShellCommandHiiHandle, L"initrd", Param);
          ShellStatus = SHELL_NOT_FOUND;
        }
        if (mInitrdLoadFile2Handle == NULL) {
          Status = gBS->InstallMultipleProtocolInterfaces (
                          &mInitrdLoadFile2Handle,
                          &gEfiDevicePathProtocolGuid,    &mInitrdDevicePath,
                          &gEfiLoadFile2ProtocolGuid,     &mInitrdLoadFile2,
                          NULL);
          ASSERT_EFI_ERROR (Status);
        }
      }
    }
  }

  return ShellStatus;
}

/**
  Constructor for the 'initrd' UEFI Shell command library

  @param ImageHandle    the image handle of the process
  @param SystemTable    the EFI System Table pointer

  @retval EFI_SUCCESS        the shell command handlers were installed sucessfully
  @retval EFI_UNSUPPORTED    the shell level required was not found.
**/
EFI_STATUS
EFIAPI
LinuxInitrdShellCommandLibConstructor (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  gLinuxInitrdShellCommandHiiHandle = HiiAddPackages (&gShellInitrdHiiGuid,
    gImageHandle, LinuxInitrdShellCommandLibStrings, NULL);
  if (gLinuxInitrdShellCommandHiiHandle == NULL) {
    return EFI_DEVICE_ERROR;
  }

  ShellCommandRegisterCommandName (L"initrd", ShellCommandRunInitrd,
    ShellCommandGetManFileNameInitrd, 0, L"initrd", TRUE,
    gLinuxInitrdShellCommandHiiHandle, STRING_TOKEN(STR_GET_HELP_INITRD));

  return EFI_SUCCESS;
}

/**
  Destructor for the library.  free any resources.

  @param ImageHandle    The image handle of the process.
  @param SystemTable    The EFI System Table pointer.

  @retval EFI_SUCCESS   Always returned.
**/
EFI_STATUS
EFIAPI
LinuxInitrdShellCommandLibDestructor (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  if (gLinuxInitrdShellCommandHiiHandle != NULL) {
    HiiRemovePackages (gLinuxInitrdShellCommandHiiHandle);
  }

  if (mInitrdLoadFile2Handle != NULL) {
      gBS->UninstallMultipleProtocolInterfaces (mInitrdLoadFile2Handle,
             &gEfiDevicePathProtocolGuid,    &mInitrdDevicePath,
             &gEfiLoadFile2ProtocolGuid,     &mInitrdLoadFile2,
             NULL);
  }
  return EFI_SUCCESS;
}
