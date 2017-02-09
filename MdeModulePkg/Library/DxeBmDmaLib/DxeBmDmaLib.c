/** @file
  DMA abstraction library APIs. Based on PCI IO protocol DMA abstractions.

  Copyright (c) 2008 - 2010, Apple Inc. All rights reserved.<BR>
  Copyright (c) 2017, AMD Inc. All rights reserved.<BR>

  This program and the accompanying materials
  are licensed and made available under the terms and conditions of the BSD License
  which accompanies this distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

  Derived from:
   MdeModulePkg/Bus/Pci/PciHostBridgeDxe/PciRootBridgeIo.c

**/

#include <PiDxe.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/DxeServicesTableLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/BmDmaLib.h>


#define FORCE_BELOW_4GB_TRUE   TRUE
#define FORCE_BELOW_4GB_FALSE  FALSE
#define NO_MAPPING             (VOID *) (UINTN) -1


typedef struct {
  BM_DMA_OPERATION      Operation;
  UINTN                 NumberOfBytes;
  UINTN                 NumberOfPages;
  EFI_PHYSICAL_ADDRESS  HostAddress;
  EFI_PHYSICAL_ADDRESS  MappedHostAddress;
} MAP_INFO;


EFI_STATUS
AllocateBounceBuffer (
  IN     BOOLEAN               ForceBelow4GB,
  IN     BM_DMA_OPERATION      Operation,
  IN     EFI_PHYSICAL_ADDRESS  HostAddress,
  IN OUT UINTN                 *NumberOfBytes,
  OUT    PHYSICAL_ADDRESS      *DeviceAddress,
  OUT    VOID                  **Mapping
  )
{
  EFI_STATUS         Status;
  MAP_INFO           *MapInfo;
  EFI_ALLOCATE_TYPE  AllocateType;

  //
  // Allocate a MAP_INFO structure to remember the mapping when Unmap() is
  // called later.
  //
  MapInfo = AllocatePool (sizeof (MAP_INFO));
  if (MapInfo == NULL) {
    *NumberOfBytes = 0;
    return EFI_OUT_OF_RESOURCES;
  }

  //
  // Initialize the MAP_INFO structure
  //
  MapInfo->Operation     = Operation;
  MapInfo->NumberOfBytes = *NumberOfBytes;
  MapInfo->NumberOfPages = EFI_SIZE_TO_PAGES (MapInfo->NumberOfBytes);
  MapInfo->HostAddress   = HostAddress;

  if (ForceBelow4GB) {
    //
    // Limit allocations to memory below 4GB
    //
    AllocateType = AllocateMaxAddress;
    MapInfo->MappedHostAddress = SIZE_4GB - 1;
  } else {
    AllocateType = AllocateAnyPages;
  }

  //
  // Allocate DMA bounce buffer
  //
  Status = gBS->AllocatePages (
                  AllocateType,
                  EfiBootServicesData,
                  MapInfo->NumberOfPages,
                  &MapInfo->MappedHostAddress
                  );

  if (EFI_ERROR (Status)) {
    FreePool (MapInfo);
    *NumberOfBytes = 0;
    return Status;
  }

  //
  // If this is a read operation from the Bus Master's point of view,
  // then copy the contents of the real buffer into the mapped buffer
  // so the Bus Master can read the contents of the real buffer.
  //
  if (Operation ==  DmaOperationBusMasterRead) {
    CopyMem (
      (VOID *) (UINTN) MapInfo->MappedHostAddress,
      (VOID *) (UINTN) MapInfo->HostAddress,
      MapInfo->NumberOfBytes
      );
  }

  //
  // The DeviceAddress is the address of the mapped buffer
  //
  *DeviceAddress = MapInfo->MappedHostAddress;

  //
  // Return a pointer to the MAP_INFO structure in Mapping
  //
  *Mapping = MapInfo;

  return EFI_SUCCESS;
}


/**
  Provides the DMA controller-specific addresses needed to access system memory.

  Operation is relative to the DMA bus master.

  @param  DmaAbove4GB           Indicates capability of DMA operations above 4GB.
  @param  Operation             Indicates if the bus master is going to read or write to system memory.
  @param  HostAddress           The system memory address to map to the DMA controller.
  @param  NumberOfBytes         On input the number of bytes to map. On output the number of bytes
                                that were mapped.
  @param  DeviceAddress         The resulting map address for the bus master controller to use to
                                access the hosts HostAddress.
  @param  Mapping               A resulting value to pass to BmDmaUnmap().

  @retval EFI_SUCCESS           The range was mapped for the returned NumberOfBytes.
  @retval EFI_UNSUPPORTED       The HostAddress cannot be mapped as a common buffer.
  @retval EFI_INVALID_PARAMETER One or more parameters are invalid.
  @retval EFI_OUT_OF_RESOURCES  The request could not be completed due to a lack of resources.
  @retval EFI_DEVICE_ERROR      The system hardware could not map the requested address.

**/
EFI_STATUS
EFIAPI
BmDmaMap (
  IN     BOOLEAN           DmaAbove4GB,
  IN     BM_DMA_OPERATION  Operation,
  IN     VOID              *HostAddress,
  IN OUT UINTN             *NumberOfBytes,
  OUT    PHYSICAL_ADDRESS  *DeviceAddress,
  OUT    VOID              **Mapping
  )
{
  EFI_PHYSICAL_ADDRESS  PhysicalAddress;

  //
  // Check for invalid inputs
  //
  if (HostAddress == NULL || NumberOfBytes == NULL || DeviceAddress == NULL ||
      Mapping == NULL || (UINT32) Operation >= DmaOperationBusMasterMaximum) {
    return EFI_INVALID_PARAMETER;
  }

  PhysicalAddress = (EFI_PHYSICAL_ADDRESS) (UINTN) HostAddress;
  if (DmaAbove4GB || (PhysicalAddress + *NumberOfBytes) <= SIZE_4GB) {
    //
    // If we CAN handle DMA above 4GB or the transfer is below 4GB,
    // the DeviceAddress is simply the HostAddress
    //
    *DeviceAddress = PhysicalAddress;
    *Mapping       = NO_MAPPING;

    return EFI_SUCCESS;
  }

  //
  // If we cannot handle DMA above 4GB and any part of the DMA transfer
  // being is above 4GB, then map the DMA transfer to a buffer below 4GB.
  //
  if (Operation == DmaOperationBusMasterCommonBuffer) {
    //
    // Common Buffer operations cannot be remapped, so return an error.
    //
    return EFI_UNSUPPORTED;
  }

  return AllocateBounceBuffer (
           Operation,
           FORCE_BELOW_4GB_TRUE,
           PhysicalAddress,
           NumberOfBytes,
           DeviceAddress,
           Mapping
           );
}


/**
  Completes the DmaOperationBusMasterRead/Write/CommonBuffer operation
  and releases any corresponding resources.

  @param  Mapping               The mapping value returned from BmDmaMap().

  @retval EFI_SUCCESS           The range was unmapped.
  @retval EFI_DEVICE_ERROR      The data was not committed to the target system memory.

**/
EFI_STATUS
EFIAPI
BmDmaUnmap (
  IN  VOID                 *Mapping
  )
{
  MAP_INFO  *MapInfo;

  //
  // Check for invalid inputs
  //
  if (Mapping == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  //
  // See if the Map() operation associated with this Unmap() required a mapping
  // buffer. If a mapping buffer was not required, then this function simply
  // returns EFI_SUCCESS.
  //
  if (Mapping == NO_MAPPING) {
    return EFI_SUCCESS;
  }

  //
  // If this is a write operation from the Bus Master's point of view,
  // then copy the contents of the mapped buffer into the real buffer
  // so the processor can read the contents of the real buffer.
  //
  MapInfo = (MAP_INFO *)Mapping;
  if (MapInfo->Operation == DmaOperationBusMasterWrite) {
    CopyMem (
      (VOID *) (UINTN) MapInfo->HostAddress,
      (VOID *) (UINTN) MapInfo->MappedHostAddress,
      MapInfo->NumberOfBytes
      );
  }

  //
  // Free the mapped buffer and the MAP_INFO structure.
  //
  gBS->FreePages (MapInfo->MappedHostAddress, MapInfo->NumberOfPages);
  FreePool (Mapping);
  return EFI_SUCCESS;
}


/**
  Allocates pages that are suitable for a BmDmaMap() of type DmaOperationBusMasterCommonBuffer.

  @param  DmaAbove4GB           Indicates capability of DMA operations above 4GB.
  @param  MemoryType            The type of memory to allocate: EfiBootServicesData or
                                EfiRuntimeServicesData.
  @param  Pages                 The number of pages to allocate.
  @param  HostAddress           A pointer to store the base system memory address of the
                                allocated range.

  @retval EFI_SUCCESS           The requested memory pages were allocated.
  @retval EFI_UNSUPPORTED       Attributes is unsupported. The only legal attribute bits are
                                MEMORY_WRITE_COMBINE and MEMORY_CACHED.
  @retval EFI_INVALID_PARAMETER One or more parameters are invalid.
  @retval EFI_OUT_OF_RESOURCES  The memory pages could not be allocated.

**/
EFI_STATUS
EFIAPI
BmDmaAllocateBuffer (
  IN  BOOLEAN              DmaAbove4GB,
  IN  EFI_MEMORY_TYPE      MemoryType,
  IN  UINTN                Pages,
  OUT VOID                 **HostAddress
  )
{
  EFI_STATUS           Status;
  EFI_PHYSICAL_ADDRESS PhysicalAddress;
  EFI_ALLOCATE_TYPE    AllocateType;

  //
  // Check for invalid inputs
  //
  if (HostAddress == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  //
  // The only valid memory types are EfiBootServicesData and
  // EfiRuntimeServicesData
  //
  if (MemoryType != EfiBootServicesData &&
      MemoryType != EfiRuntimeServicesData) {
    return EFI_INVALID_PARAMETER;
  }

  if (DmaAbove4GB) {
    AllocateType = AllocateAnyPages;
  } else {
    //
    // Limit allocations to memory below 4GB
    //
    AllocateType    = AllocateMaxAddress;
    PhysicalAddress = (EFI_PHYSICAL_ADDRESS) (SIZE_4GB - 1);
  }
  Status = gBS->AllocatePages (
                  AllocateType,
                  MemoryType,
                  Pages,
                  &PhysicalAddress
                  );
  if (!EFI_ERROR (Status)) {
    *HostAddress = (VOID *) (UINTN) PhysicalAddress;
  }

  return Status;
}


/**
  Frees memory that was allocated with BmDmaAllocateBuffer().

  @param  Pages                 The number of pages to free.
  @param  HostAddress           The base system memory address of the allocated range.

  @retval EFI_SUCCESS           The requested memory pages were freed.
  @retval EFI_INVALID_PARAMETER The memory range specified by HostAddress and Pages
                                was not allocated with BmDmaAllocateBuffer().

**/
EFI_STATUS
EFIAPI
BmDmaFreeBuffer (
  IN  UINTN                Pages,
  IN  VOID                 *HostAddress
  )
{
  return gBS->FreePages ((EFI_PHYSICAL_ADDRESS) (UINTN) HostAddress, Pages);
}
