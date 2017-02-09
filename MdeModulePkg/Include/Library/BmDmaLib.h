/** @file
  DMA abstraction library APIs. Based on PCI IO protocol DMA abstractions.

  Copyright (c) 2008 - 2010, Apple Inc. All rights reserved.<BR>
  Copyright (c) 2017, AMD Inc. All rights reserved.<BR>

  DMA Bus Master Read Operation:
    Call BmDmaMap() for DmaOperationBusMasterRead.
    Program the DMA Bus Master with the DeviceAddress returned by BmDmaMap().
    Start the DMA Bus Master.
    Wait for DMA Bus Master to complete the read operation.
    Call BmDmaUnmap().

  DMA Bus Master Write Operation:
    Call BmDmaMap() for DmaOperationBusMasterWrite.
    Program the DMA Bus Master with the DeviceAddress returned by BmDmaMap().
    Start the DMA Bus Master.
    Wait for DMA Bus Master to complete the write operation.
    Call BmDmaUnmap().

  DMA Bus Master Common Buffer Operation:
    Call BmDmaAllocateBuffer() to allocate a common buffer.
    Call BmDmaMap() for DmaOperationBusMasterCommonBuffer.
    Program the DMA Bus Master with the DeviceAddress returned by BmDmaMap().
    The common buffer can now be accessed equally by the processor and the DMA bus master.
    Call BmDmaUnmap().
    Call BmDmaFreeBuffer().

  This program and the accompanying materials
  are licensed and made available under the terms and conditions of the BSD License
  which accompanies this distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

 Derived from:
   EmbeddedPkg/Include/Library/DmaLib.h

**/

#ifndef __BM_DMA_LIB_H__
#define __BM_DMA_LIB_H__


typedef enum {
  ///
  /// A read operation from system memory by a bus master.
  ///
  DmaOperationBusMasterRead,
  ///
  /// A write operation from system memory by a bus master.
  ///
  DmaOperationBusMasterWrite,
  ///
  /// Provides both read and write access to system memory by both the processor and a
  /// bus master. The buffer is coherent from both the processor's and the bus master's point of view.
  ///
  DmaOperationBusMasterCommonBuffer,
  DmaOperationBusMasterMaximum
} BM_DMA_OPERATION;


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
  );


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
  );


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
  );


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
  );


#endif

