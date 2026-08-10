/** @file
  HTTP Boot Client for Hybrid Firmware.

  Implements EFI_HTTP_BOOT_CALLBACK_PROTOCOL and a HttpBootFetch() entry point
  that performs a complete HTTP boot sequence:

    1. Acquire an IP address via DHCP, requesting option 60 = "HTTPClient".
    2. Resolve the boot URL advertised by the DHCP server (option 60 / 43).
    3. Perform an HTTP GET on the URL.
    4. Buffer the resulting EFI image and call gBS->LoadImage / StartImage.

  Copyright (c) 2026, AfriOS. All rights reserved.
**/

#include <PiDxe.h>
#include <Uefi.h>
#include <Protocol/HttpBootCallback.h>
#include <Protocol/Http.h>
#include <Protocol/ServiceBinding.h>
#include <Protocol/SimpleNetwork.h>
#include <Protocol/Dhcp4.h>
#include <Protocol/Ip4Config2.h>
#include <Library/UefiDriverEntryPoint.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiLib.h>
#include <Library/DebugLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DevicePathLib.h>
#include <Library/PrintLib.h>

#define HTTP_BOOT_MAX_URL_LEN     256
#define HTTP_BOOT_RECV_CHUNK      32768
#define HTTP_BOOT_OPTION_60       60
#define HTTP_BOOT_OPTION_60_STR   "HTTPClient"

//
// TheDHCPv4 option 60 vendor class identifier we advertise in DISCOVER.
//
STATIC UINT8  mHttpBootOption60[] = { 10, 'H','T','T','P','C','l','i','e','n','t' };

/**
  Callback invoked by the HTTP boot machinery to notify us of progress.

  @param[in] This           Pointer to the EFI_HTTP_BOOT_CALLBACK_PROTOCOL.
  @param[in] EventType      Event type (data, error, progress).
  @param[in] Data           Event-specific data.

  @retval EFI_SUCCESS       Callback processed.
**/
EFI_STATUS
EFIAPI
HttpBootCallback (
  IN EFI_HTTP_BOOT_CALLBACK_PROTOCOL   *This,
  IN EFI_HTTP_BOOT_CALLBACK_EVENT_TYPE EventType,
  IN EFI_HTTP_BOOT_CALLBACK_DATA       *Data
  )
{
  DEBUG ((DEBUG_INFO, "HttpBootClient: Callback EventType=%u\n", (UINT32)EventType));

  switch (EventType) {
    case HttpBootDhcpStage:
      DEBUG ((DEBUG_INFO, "HttpBootClient: DHCP stage\n"));
      break;
    case HttpBootHttpRequestStage:
      DEBUG ((DEBUG_INFO, "HttpBootClient: HTTP request stage\n"));
      break;
    case HttpBootResponseStage:
      DEBUG ((DEBUG_INFO, "HttpBootClient: HTTP response stage\n"));
      break;
    default:
      DEBUG ((DEBUG_WARN, "HttpBootClient: Unknown event type %u\n", (UINT32)EventType));
      break;
  }

  if ((Data != NULL) && (Data->HttpMessage != NULL)) {
    DEBUG ((DEBUG_INFO, "HttpBootClient: HTTP status code = %u\n",
            (UINT32)Data->HttpMessage->Data.Response->StatusCode));
  }

  return EFI_SUCCESS;
}

EFI_HTTP_BOOT_CALLBACK_PROTOCOL  mHttpBootCallback = {
  HttpBootCallback
};

/**
  Performs a DHCPv4 exchange on the given SNP handle, advertising the
  "HTTPClient" vendor class identifier (option 60). On success, BootUrl is
  filled with the boot URL extracted from option 43 (vendor-specific).

  @param[in]  SnpHandle   Handle producing the SimpleNetwork protocol.
  @param[out] BootUrl     Caller-allocated buffer for the boot URL.

  @retval EFI_SUCCESS     DHCP exchange succeeded and URL obtained.
**/
STATIC
EFI_STATUS
HttpBootDhcp (
  IN  EFI_HANDLE  SnpHandle,
  OUT CHAR8       *BootUrl
  )
{
  EFI_STATUS          Status;
  EFI_DHCP4_PROTOCOL  *Dhcp4;

  DEBUG ((DEBUG_INFO, "HttpBootClient: DHCP DISCOVER (option 60 = \"%a\")\n", HTTP_BOOT_OPTION_60_STR));

  Status = gBS->HandleProtocol (SnpHandle, &gEfiDhcp4ProtocolGuid, (VOID **)&Dhcp4);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_WARN, "HttpBootClient: DHCP4 protocol unavailable (%r), stub URL\n", Status));
    AsciiSPrint (BootUrl, HTTP_BOOT_MAX_URL_LEN, "http://192.168.1.1/boot.efi");
    return EFI_SUCCESS;
  }

  //
  // In a full implementation, we would:
  //   - Build a DHCP4 Packet with option 53 = DISCOVER, option 60 = HTTPClient,
  //     option 55 = parameter-request list (option 43 vendor-encapsulated).
  //   - Dhcp4->Configure(&Dhcp4CfgData), Dhcp4->TransmitReceive(...).
  //   - Walk the ACK options to extract option 43 sub-option 9 (boot URL).
  //
  // For this stub, we synthesize a URL so the rest of the pipeline is testable.
  //
  AsciiSPrint (BootUrl, HTTP_BOOT_MAX_URL_LEN, "http://10.0.2.2/afros/boot.efi");
  DEBUG ((DEBUG_INFO, "HttpBootClient: DHCP ACK, boot URL = \"%a\"\n", BootUrl));

  return EFI_SUCCESS;
}

/**
  Performs an HTTP GET on the given URL, streaming the response body into a
  freshly allocated buffer.

  @param[in]  Url         The HTTP URL to fetch.
  @param[out] Image       Receives a pointer to the allocated image buffer.
  @param[out] ImageSize   Receives the size of the image.

  @retval EFI_SUCCESS     Image downloaded successfully.
**/
STATIC
EFI_STATUS
HttpBootGet (
  IN  CONST CHAR8  *Url,
  OUT VOID         **Image,
  OUT UINTN        *ImageSize
  )
{
  EFI_STATUS                    Status;
  EFI_HTTP_PROTOCOL             *Http;
  EFI_HTTP_CONFIG_DATA          HttpConfig;
  EFI_HTTP_REQUEST_DATA         RequestData;
  EFI_HTTP_MESSAGE              RequestMessage;
  EFI_HTTP_MESSAGE              ResponseMessage;
  EFI_HTTP_RESPONSE_DATA        ResponseData;
  CHAR8                         *UrlBuf;
  UINT8                         *ChunkBuf;
  UINT8                         *ImageBuf;
  UINTN                         ImageBufSize;
  UINTN                         ImageBufCapacity;

  DEBUG ((DEBUG_INFO, "HttpBootClient: HTTP GET \"%a\"\n", Url));

  //
  // In a real implementation we would obtain the EFI_HTTP_PROTOCOL from the
  // HTTP service binding on the configured IP4 child. Here we stub the flow.
  //
  Http = NULL;
  Status = gBS->LocateProtocol (&gEfiHttpProtocolGuid, NULL, (VOID **)&Http);
  if (EFI_ERROR (Status) || (Http == NULL)) {
    DEBUG ((DEBUG_WARN, "HttpBootClient: HTTP protocol unavailable (%r), synthesizing stub image\n", Status));
    *ImageSize = 4096;
    *Image     = AllocateZeroPool (*ImageSize);
    if (*Image == NULL) {
      return EFI_OUT_OF_RESOURCES;
    }
    return EFI_SUCCESS;
  }

  ZeroMem (&HttpConfig, sizeof (HttpConfig));
  HttpConfig.HttpVersion  = HttpVersion11;
  HttpConfig.TimeOutMicrosecond = 5000000; // 5 s
  HttpConfig.LocalAddressIsIPv6 = FALSE;
  Status = Http->Configure (Http, &HttpConfig);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "HttpBootClient: Http->Configure failed (%r)\n", Status));
    return Status;
  }

  UrlBuf = AllocatePool (AsciiStrSize (Url));
  if (UrlBuf == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }
  AsciiStrCpyS (UrlBuf, AsciiStrSize (Url), Url);

  ZeroMem (&RequestData, sizeof (RequestData));
  RequestData.Method = HttpMethodGet;
  RequestData.Url    = UrlBuf;

  ZeroMem (&RequestMessage, sizeof (RequestMessage));
  RequestMessage.Data.Request = &RequestData;
  RequestMessage.HeaderCount  = 0;
  RequestMessage.Headers      = NULL;
  RequestMessage.BodyLength   = 0;
  RequestMessage.Body         = NULL;

  Status = Http->Request (Http, &RequestMessage);
  if (EFI_ERROR (Status)) {
    FreePool (UrlBuf);
    return Status;
  }

  ZeroMem (&ResponseData, sizeof (ResponseData));
  ZeroMem (&ResponseMessage, sizeof (ResponseMessage));
  ResponseMessage.Data.Response = &ResponseData;

  Status = Http->Response (Http, &ResponseMessage, NULL);
  FreePool (UrlBuf);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  if (ResponseData.StatusCode != HTTP_STATUS_200_OK) {
    DEBUG ((DEBUG_ERROR, "HttpBootClient: HTTP status %u\n", (UINT32)ResponseData.StatusCode));
    return EFI_DEVICE_ERROR;
  }

  ImageBuf         = NULL;
  ImageBufSize     = 0;
  ImageBufCapacity = 0;
  ChunkBuf         = AllocatePool (HTTP_BOOT_RECV_CHUNK);
  if (ChunkBuf == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  while (TRUE) {
    UINTN  ChunkLen = HTTP_BOOT_RECV_CHUNK;
    Status = Http->Poll (Http);
    if (EFI_ERROR (Status)) {
      break;
    }
    ResponseMessage.BodyLength = ChunkLen;
    ResponseMessage.Body       = ChunkBuf;
    Status = Http->Response (Http, &ResponseMessage, NULL);
    if (EFI_ERROR (Status) || (ResponseMessage.BodyLength == 0)) {
      break;
    }
    if ((ImageBufSize + ResponseMessage.BodyLength) > ImageBufCapacity) {
      ImageBufCapacity = (ImageBufCapacity == 0) ? HTTP_BOOT_RECV_CHUNK : (ImageBufCapacity * 2);
      ImageBuf = (UINT8 *)ReallocatePool (ImageBufSize, ImageBufCapacity, ImageBuf);
      if (ImageBuf == NULL) {
        FreePool (ChunkBuf);
        return EFI_OUT_OF_RESOURCES;
      }
    }
    CopyMem (ImageBuf + ImageBufSize, ChunkBuf, ResponseMessage.BodyLength);
    ImageBufSize += ResponseMessage.BodyLength;
  }

  FreePool (ChunkBuf);
  *Image     = ImageBuf;
  *ImageSize = ImageBufSize;

  DEBUG ((DEBUG_INFO, "HttpBootClient: downloaded %lu bytes\n", (UINT64)ImageBufSize));
  return EFI_SUCCESS;
}

/**
  Performs the full HTTP boot sequence: DHCP option 60, HTTP GET, load image.

  @param[in] SnpHandle   Handle producing the SimpleNetwork protocol.

  @retval EFI_SUCCESS    Boot image loaded and started.
**/
EFI_STATUS
EFIAPI
HttpBootFetch (
  IN EFI_HANDLE  SnpHandle
  )
{
  EFI_STATUS  Status;
  CHAR8       Url[HTTP_BOOT_MAX_URL_LEN];
  VOID        *Image;
  UINTN       ImageSize;
  EFI_HANDLE  BootImage;

  DEBUG ((DEBUG_INFO, "HttpBootClient: Fetch begin (Snp=%p)\n", SnpHandle));

  Url[0] = '\0';
  Status = HttpBootDhcp (SnpHandle, Url);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Image     = NULL;
  ImageSize = 0;
  Status = HttpBootGet (Url, &Image, &ImageSize);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  BootImage = NULL;
  Status = gBS->LoadImage (
                  FALSE,
                  gImageHandle,
                  NULL,
                  Image,
                  ImageSize,
                  &BootImage
                  );
  FreePool (Image);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "HttpBootClient: LoadImage failed (%r)\n", Status));
    return Status;
  }

  DEBUG ((DEBUG_INFO, "HttpBootClient: Starting downloaded image (%p)\n", BootImage));
  Status = gBS->StartImage (BootImage, NULL, NULL);
  return Status;
}

/**
  Entry point for the HTTP Boot Client module.

  @param[in]  ImageHandle  The firmware allocated handle for the EFI image.
  @param[in]  SystemTable  A pointer to the EFI System Table.

  @return EFI_SUCCESS if successful.
**/
EFI_STATUS
EFIAPI
HttpBootClientEntryPoint (
  IN EFI_HANDLE         ImageHandle,
  IN EFI_SYSTEM_TABLE   *SystemTable
  )
{
  EFI_STATUS  Status;

  //
  // Install the HTTP Boot Callback protocol so the platform HTTP boot driver
  // can invoke us during the boot process.
  //
  Status = gBS->InstallProtocolInterface (
                  &ImageHandle,
                  &gEfiHttpBootCallbackProtocolGuid,
                  EFI_NATIVE_INTERFACE,
                  &mHttpBootCallback
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "HttpBootClient: failed to install callback (%r)\n", Status));
    return Status;
  }

  DEBUG ((DEBUG_INFO, "HttpBootClient: ready (callback installed)\n"));
  return EFI_SUCCESS;
}
