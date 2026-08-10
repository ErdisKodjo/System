/** @file
  iPXE Driver Binding for Hybrid Firmware PXE Boot.

  This module implements the EFI_DRIVER_BINDING_PROTOCOL for an iPXE-based
  network boot driver. The driver binds onto a handle that produces both
  EFI_SIMPLE_NETWORK_PROTOCOL (or the EFI_HTTP_SERVICE_BINDING_PROTOCOL) and
  a device path, then attaches an iPXE boot capability to that handle.

  Copyright (c) 2026, AfriOS. All rights reserved.
**/

#include <PiDxe.h>
#include <Uefi.h>
#include <Protocol/DriverBinding.h>
#include <Protocol/SimpleNetwork.h>
#include <Protocol/ServiceBinding.h>
#include <Protocol/DevicePath.h>
#include <Protocol/ComponentName2.h>
#include <Protocol/ComponentName.h>
#include <Library/UefiDriverEntryPoint.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiLib.h>
#include <Library/DebugLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DevicePathLib.h>

#define IPXE_DRIVER_PRIVATE_SIGNATURE  SIGNATURE_32 ('i','P','X','E')

//
// Private context attached to each child handle produced by this driver.
//
typedef struct {
  UINT32                          Signature;
  EFI_HANDLE                      Controller;
  EFI_SIMPLE_NETWORK_PROTOCOL     *Snp;
  EFI_DEVICE_PATH_PROTOCOL        *DevicePath;
  BOOLEAN                         HttpBootCapable;
} IPXE_DRIVER_PRIVATE;

//
// Forward declarations for the three DriverBinding callbacks.
//
EFI_STATUS
EFIAPI
IpxeDriverSupported (
  IN EFI_DRIVER_BINDING_PROTOCOL  *This,
  IN EFI_HANDLE                   ControllerHandle,
  IN EFI_DEVICE_PATH_PROTOCOL     *RemainingDevicePath OPTIONAL
  );

EFI_STATUS
EFIAPI
IpxeDriverStart (
  IN EFI_DRIVER_BINDING_PROTOCOL  *This,
  IN EFI_HANDLE                   ControllerHandle,
  IN EFI_DEVICE_PATH_PROTOCOL     *RemainingDevicePath OPTIONAL
  );

EFI_STATUS
EFIAPI
IpxeDriverStop (
  IN  EFI_DRIVER_BINDING_PROTOCOL  *This,
  IN  EFI_HANDLE                   ControllerHandle,
  IN  UINTN                        NumberOfChildren,
  IN  EFI_HANDLE                   *ChildHandleBuffer OPTIONAL
  );

//
// Driver Binding protocol instance.
//
EFI_DRIVER_BINDING_PROTOCOL  mIpxeDriverBinding = {
  IpxeDriverSupported,
  IpxeDriverStart,
  IpxeDriverStop,
  0x10,
  NULL,
  NULL
};

/**
  Reports whether the driver supports a given controller. We support any
  controller that exposes the Simple Network Protocol and a device path.

  @param[in] This                 Pointer to the DriverBinding instance.
  @param[in] ControllerHandle     Handle of the controller to test.
  @param[in] RemainingDevicePath  Optional remaining device path.

  @retval EFI_SUCCESS         Driver supports this controller.
  @retval EFI_UNSUPPORTED     Driver does not support this controller.
**/
EFI_STATUS
EFIAPI
IpxeDriverSupported (
  IN EFI_DRIVER_BINDING_PROTOCOL  *This,
  IN EFI_HANDLE                   ControllerHandle,
  IN EFI_DEVICE_PATH_PROTOCOL     *RemainingDevicePath OPTIONAL
  )
{
  EFI_STATUS                    Status;
  EFI_SIMPLE_NETWORK_PROTOCOL   *Snp;
  EFI_DEVICE_PATH_PROTOCOL      *DevicePath;

  //
  // We require both SNP and a device path. SNP gives us the wire, the device
  // path lets us record the network device for the boot manager.
  //
  Status = gBS->HandleProtocol (
                  ControllerHandle,
                  &gEfiSimpleNetworkProtocolGuid,
                  (VOID **)&Snp
                  );
  if (EFI_ERROR (Status)) {
    return EFI_UNSUPPORTED;
  }

  Status = gBS->HandleProtocol (
                  ControllerHandle,
                  &gEfiDevicePathProtocolGuid,
                  (VOID **)&DevicePath
                  );
  if (EFI_ERROR (Status)) {
    return EFI_UNSUPPORTED;
  }

  DEBUG ((DEBUG_INFO, "IpxeDriver: Supports controller %p (Snp=%p)\n", ControllerHandle, Snp));
  return EFI_SUCCESS;
}

/**
  Starts the iPXE driver on the given controller: opens SNP and DevicePath
  BY_DRIVER, allocates a private context, and records it on the controller.

  @param[in] This                 Pointer to the DriverBinding instance.
  @param[in] ControllerHandle     Handle of the controller to start.
  @param[in] RemainingDevicePath  Optional remaining device path.

  @retval EFI_SUCCESS             Driver started successfully.
**/
EFI_STATUS
EFIAPI
IpxeDriverStart (
  IN EFI_DRIVER_BINDING_PROTOCOL  *This,
  IN EFI_HANDLE                   ControllerHandle,
  IN EFI_DEVICE_PATH_PROTOCOL     *RemainingDevicePath OPTIONAL
  )
{
  EFI_STATUS                    Status;
  EFI_SIMPLE_NETWORK_PROTOCOL   *Snp;
  EFI_DEVICE_PATH_PROTOCOL      *DevicePath;
  IPXE_DRIVER_PRIVATE           *Private;

  DEBUG ((DEBUG_INFO, "IpxeDriver: Start on controller %p\n", ControllerHandle));

  Status = gBS->OpenProtocol (
                  ControllerHandle,
                  &gEfiSimpleNetworkProtocolGuid,
                  (VOID **)&Snp,
                  This->DriverBindingHandle,
                  ControllerHandle,
                  EFI_OPEN_PROTOCOL_BY_DRIVER
                  );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = gBS->OpenProtocol (
                  ControllerHandle,
                  &gEfiDevicePathProtocolGuid,
                  (VOID **)&DevicePath,
                  This->DriverBindingHandle,
                  ControllerHandle,
                  EFI_OPEN_PROTOCOL_BY_DRIVER
                  );
  if (EFI_ERROR (Status)) {
    gBS->CloseProtocol (
           ControllerHandle,
           &gEfiSimpleNetworkProtocolGuid,
           This->DriverBindingHandle,
           ControllerHandle
           );
    return Status;
  }

  Private = (IPXE_DRIVER_PRIVATE *)AllocateZeroPool (sizeof (*Private));
  if (Private == NULL) {
    gBS->CloseProtocol (ControllerHandle, &gEfiDevicePathProtocolGuid, This->DriverBindingHandle, ControllerHandle);
    gBS->CloseProtocol (ControllerHandle, &gEfiSimpleNetworkProtocolGuid, This->DriverBindingHandle, ControllerHandle);
    return EFI_OUT_OF_RESOURCES;
  }

  Private->Signature      = IPXE_DRIVER_PRIVATE_SIGNATURE;
  Private->Controller     = ControllerHandle;
  Private->Snp            = Snp;
  Private->DevicePath     = DevicePath;
  Private->HttpBootCapable = (Snp->Mode != NULL) && (Snp->Mode->State == EfiSimpleNetworkInitialized);

  Status = gBS->InstallProtocolInterface (
                  &ControllerHandle,
                  &gEfiCallerIdGuid,
                  EFI_NATIVE_INTERFACE,
                  Private
                  );
  if (EFI_ERROR (Status)) {
    FreePool (Private);
    gBS->CloseProtocol (ControllerHandle, &gEfiDevicePathProtocolGuid, This->DriverBindingHandle, ControllerHandle);
    gBS->CloseProtocol (ControllerHandle, &gEfiSimpleNetworkProtocolGuid, This->DriverBindingHandle, ControllerHandle);
    return Status;
  }

  DEBUG ((DEBUG_INFO, "IpxeDriver: Started (Private=%p, HttpBootCapable=%d)\n",
          Private, (UINT32)Private->HttpBootCapable));
  return EFI_SUCCESS;
}

/**
  Stops the iPXE driver on the given controller: uninstalls our private
  protocol, frees the context, and closes the opened protocols.

  @param[in] This                Pointer to the DriverBinding instance.
  @param[in] ControllerHandle    Handle of the controller to stop.
  @param[in] NumberOfChildren    Number of child handles (unused).
  @param[in] ChildHandleBuffer   Array of child handles (unused).

  @retval EFI_SUCCESS            Driver stopped successfully.
**/
EFI_STATUS
EFIAPI
IpxeDriverStop (
  IN  EFI_DRIVER_BINDING_PROTOCOL  *This,
  IN  EFI_HANDLE                   ControllerHandle,
  IN  UINTN                        NumberOfChildren,
  IN  EFI_HANDLE                   *ChildHandleBuffer OPTIONAL
  )
{
  EFI_STATUS            Status;
  IPXE_DRIVER_PRIVATE   *Private;

  DEBUG ((DEBUG_INFO, "IpxeDriver: Stop on controller %p\n", ControllerHandle));

  Status = gBS->HandleProtocol (
                  ControllerHandle,
                  &gEfiCallerIdGuid,
                  (VOID **)&Private
                  );
  if (EFI_ERROR (Status) || (Private == NULL) || (Private->Signature != IPXE_DRIVER_PRIVATE_SIGNATURE)) {
    return EFI_DEVICE_ERROR;
  }

  gBS->UninstallProtocolInterface (
         ControllerHandle,
         &gEfiCallerIdGuid,
         Private
         );

  gBS->CloseProtocol (ControllerHandle, &gEfiDevicePathProtocolGuid, This->DriverBindingHandle, ControllerHandle);
  gBS->CloseProtocol (ControllerHandle, &gEfiSimpleNetworkProtocolGuid, This->DriverBindingHandle, ControllerHandle);

  FreePool (Private);
  return EFI_SUCCESS;
}

//
// Component Name 2 — minimal English/Italian strings.
//
STATIC EFI_UNICODE_STRING_TABLE  mIpxeDriverNameTable[] = {
  { "eng;en", L"iPXE Network Boot Driver" },
  { "fra;fr", L"Pilote iPXE de demarrage reseau" },
  { NULL,     NULL                          }
};

STATIC EFI_UNICODE_STRING_TABLE  mIpxeControllerNameTable[] = {
  { "eng;en", L"iPXE Network Boot Controller" },
  { "fra;fr", L"Controleur iPXE de demarrage reseau" },
  { NULL,     NULL                                }
};

EFI_COMPONENT_NAME2_PROTOCOL  gIpxeComponentName2 = {
  (EFI_COMPONENT_NAME2_GET_DRIVER_NAME)    NULL,
  (EFI_COMPONENT_NAME2_GET_CONTROLLER_NAME)NULL,
  "eng"
};

/**
  Entry point for the iPXE Driver.

  @param[in]  ImageHandle  The firmware allocated handle for the EFI image.
  @param[in]  SystemTable  A pointer to the EFI System Table.

  @return EFI_SUCCESS if successful.
**/
EFI_STATUS
EFIAPI
IpxeDriverEntryPoint (
  IN EFI_HANDLE         ImageHandle,
  IN EFI_SYSTEM_TABLE   *SystemTable
  )
{
  EFI_STATUS  Status;

  mIpxeDriverBinding.ImageHandle     = ImageHandle;
  mIpxeDriverBinding.DriverBindingHandle = ImageHandle;

  Status = gBS->InstallMultipleProtocolInterfaces (
                  &ImageHandle,
                  &gEfiDriverBindingProtocolGuid, &mIpxeDriverBinding,
                  &gEfiComponentName2ProtocolGuid, &gIpxeComponentName2,
                  NULL
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "IpxeDriver: Failed to install protocols (%r)\n", Status));
    return Status;
  }

  DEBUG ((DEBUG_INFO, "IpxeDriver: Installed DriverBinding on %p\n", ImageHandle));
  return EFI_SUCCESS;
}
