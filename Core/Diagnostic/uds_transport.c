#include "uds_services.h"
#include "uds_transport.h"
#include "CanTp.h"
#include <string.h>

static void (*after_response)(void); /* Optional reset/power action awaiting TX completion. */
static bool response_accepted;      /* Whether the last send copied a payload successfully. */

/* Defer disruptive ECU actions until the asynchronous response reaches the bus. */
void UDS_AfterResponse(void (*action)(void))
{
  if (response_accepted) { after_response = action; }
}

/* Execute a pending action only on successful final CAN confirmation. */
void CanTp_TxComplete(bool success)
{
  void (*action)(void) = after_response;
  after_response = NULL;
  response_accepted = false;
  if (success && action != NULL) { action(); }
}

/* Submit a whole UDS response; CanTp owns the copied bytes after this call. */
void send_can_message(const uint8_t *message, uint16_t length)
{
  response_accepted = CanTp_Transmit(message, length, HAL_GetTick()) == CANTP_OK;
  if (!response_accepted)
  {
    UART_Send("[CanTp] response rejected (busy or invalid length)\r\n");
  }
}

/* Read a network-order 16-bit field without unaligned structure casts. */
static uint16_t ReadBe16(const uint8_t *data)
{
  return (uint16_t)(((uint16_t)data[0] << 8) | data[1]);
}

/* Report a request length that cannot be represented by the legacy handler. */
static void InvalidLength(uint8_t sid)
{
  const uint8_t response[] = { UDS_NEGATIVE_RESPONSE, sid, NRC_INCORRECT_MESSAGE_LENGTH };
  send_can_message(response, sizeof(response));
}

/* Decode bounded file-transfer fields; stop parsing on error and exit once. */
static bool DecodeFileRequest(const uint8_t *data, uint16_t length, RequestFileTransfer_t *request)
{
  bool valid = true;
  uint16_t name_length;
  uint16_t index = 0U;
  uint16_t size;

  if (length < 4U)
  {
    valid = false;
  }

  /* The fixed header must be present before any field is read. */
  if (valid)
  {
    request->modeOfOperation = data[1];
    request->filePathAndNameLength = ReadBe16(data + 2);
    name_length = request->filePathAndNameLength;
    if (name_length > sizeof(request->filePathAndName) || length < 4U + name_length)
    {
      valid = false;
    }
    else
    {
      memcpy(request->filePathAndName, data + 4, name_length);
      index = 4U + name_length;
    }
  }

  if (valid)
  {
    if (request->modeOfOperation == 2U || request->modeOfOperation == 5U)
    {
      /* DeleteFile and ReadDir end immediately after the path. */
      if (index != length) { valid = false; }
    }
    else if (index >= length)
    {
      valid = false;
    }
    else
    {
      request->dataFormatIdentifier = data[index++];
      if (request->modeOfOperation == 4U)
      {
        /* ReadFile carries a data format but no file-size fields. */
        if (index != length) { valid = false; }
      }
      else if (index >= length)
      {
        valid = false;
      }
      else
      {
        request->fileSizeParameterLength = data[index++];
        size = request->fileSizeParameterLength;
        if (size == 0U || size > 4U || index + 2U * size != length)
        {
          valid = false;
        }
        else
        {
          memcpy(request->fileSizeUncompressed, data + index, size);
          memcpy(request->fileSizeCompressed, data + index + size, size);
        }
      }
    }
  }

  return valid;
}

/* Dispatch only complete ISO-TP payloads, never PCI bytes or incomplete CAN frames.
 * Most original handlers take an 8-bit parameter length: reject excess bytes
 * explicitly instead of truncating a multi-frame request to that type. */
void CanTp_RxComplete(uint8_t *data, uint16_t length)
{
  if (data == NULL || length == 0U) { return; }
  uint8_t sid = data[0];
  bool typed_transfer = sid == UDS_TRANSFER_DATA || sid == UDS_REQUEST_TRANSFER_EXIT ||
                        sid == UDS_REQUEST_FILE_TRANSFER;
  if ((!typed_transfer && length > 256U) || (length < 2U && sid != UDS_REQUEST_TRANSFER_EXIT))
  {
    InvalidLength(sid);
    return;
  }

  switch (sid)
  {
    case UDS_DIAGNOSTIC_SESSION_CONTROL:
      if (length != 2U) { InvalidLength(sid); return; }
      uds_diagnostic_session_control(data[1]);
      break;
    case UDS_ECU_RESET:
      if (length != 2U) { InvalidLength(sid); return; }
      uds_ecu_reset(data[1]);
      break;
    case UDS_SECURITY_ACCESS:
      uds_security_access(data[1], data + 2, (uint8_t)(length - 2U));
      break;
    case UDS_COMMUNICATION_CONTROL:
      uds_communication_control(data[1]);
      break;
    case UDS_TESTER_PRESENT:
      if (length != 2U) { InvalidLength(sid); return; }
      uds_tester_present(data[1]);
      break;
    case UDS_ACCESS_TIMING_PARAMETER:
      uds_access_timing_parameter(data[1], data + 2, (uint8_t)(length - 2U));
      break;
    case UDS_SECURED_DATA_TRANSMISSION:
      uds_secured_data_transmission(data + 1, (uint8_t)(length - 1U));
      break;
    case UDS_CONTROL_DTC_SETTING:
      uds_control_dtc_setting(data[1]);
      break;
    case UDS_RESPONSE_ON_EVENT:
      uds_response_on_event(data[1], data + 2, (uint8_t)(length - 2U));
      break;
    case UDS_LINK_CONTROL:
      uds_link_control(data[1], data + 2, (uint8_t)(length - 2U));
      break;
    case UDS_READ_DATA_BY_IDENTIFIER:
      uds_read_data_by_identifier(data + 1, (uint8_t)(length - 1U));
      break;
    case UDS_READ_DATA_BY_PERIODIC_IDENTIFIER:
      uds_read_data_by_periodic_identifier(data + 1, (uint8_t)(length - 1U));
      break;
    case UDS_DYNAMICAL_DEFINE_DATA_IDENTIFIER:
      uds_dynamically_define_data_identifier(data[1], data + 2, (uint8_t)(length - 2U));
      break;
    case UDS_WRITE_DATA_BY_IDENTIFIER:
      uds_write_data_by_identifier(data + 1, (uint8_t)(length - 1U));
      break;
    case UDS_CLEAR_DIAGNOSTIC_INFORMATION:
      if (length != 4U) { InvalidLength(sid); return; }
      uds_clear_diagnostic_information(data + 1, (uint8_t)(length - 1U));
      break;
    case UDS_READ_DTC_INFORMATION:
      uds_read_dtc_information(data[1], data + 2, (uint8_t)(length - 2U));
      break;
    case UDS_INPUT_OUTPUT_CONTROL_BY_IDENTIFIER:
    {
      IOControlRequest_t request = {0};
      IOControlResponse_t response = {0};
      if (length < 4U || length - 4U > sizeof(request.controlState)) { InvalidLength(sid); return; }
      request.SID = sid;
      request.dataIdentifier = ReadBe16(data + 1);
      request.controlOptionRecord = data[3];
      memcpy(request.controlState, data + 4, length - 4U);
      uds_input_output_control_by_identifier(&request, &response);
      break;
    }
    case UDS_ROUTINE_CONTROL:
    {
      RoutineControlRequest_t request = {0};
      RoutineControlResponse_t response = {0};
      if (length < 4U || length > 5U) { InvalidLength(sid); return; }
      request.subFunction = data[1];
      request.routineIdentifier = ReadBe16(data + 2);
      if (length == 5U) { request.routineControlOption = data[4]; }
      uds_routine_control(&request, &response);
      break;
    }
    case UDS_REQUEST_DOWNLOAD:
    case UDS_REQUEST_UPLOAD:
    {
      if (length < 4U) { InvalidLength(sid); return; }
      uint8_t address_size = data[2] & 0x0FU;
      uint8_t size_size = data[2] >> 4;
      if (address_size == 0U || address_size > 4U || size_size == 0U || size_size > 4U ||
          length != 3U + address_size + size_size) { InvalidLength(sid); return; }
      if (sid == UDS_REQUEST_DOWNLOAD)
      {
        RequestDownload_t request = {0};
        request.dataFormatIdentifier = data[1];
        request.addressAndLengthFormatIdentifier = data[2];
        memcpy(request.memoryAddress, data + 3, address_size);
        memcpy(request.memorySize, data + 3 + address_size, size_size);
        uds_request_download(&request);
      }
      else
      {
        RequestUpload_t request = {0};
        request.dataFormatIdentifier = data[1];
        request.addressAndLengthFormatIdentifier = data[2];
        memcpy(request.memoryAddress, data + 3, address_size);
        memcpy(request.memorySize, data + 3 + address_size, size_size);
        uds_request_upload(&request);
      }
      break;
    }
    case UDS_TRANSFER_DATA:
    {
      RequestTransferData_t request = {0};
      if (length < 2U || length - 2U > sizeof(request.transferRequestParameterRecord)) { InvalidLength(sid); return; }
      request.blockSequenceCounter = data[1];
      memcpy(request.transferRequestParameterRecord, data + 2, length - 2U);
      uds_transfer_data(&request);
      break;
    }
    case UDS_REQUEST_TRANSFER_EXIT:
    {
      RequestTransferExit_t request = {0};
      ResponseTransferExit_t response = {0};
      if (length - 1U > sizeof(request.transferRequestParameterRecord)) { InvalidLength(sid); return; }
      memcpy(request.transferRequestParameterRecord, data + 1, length - 1U);
      uds_request_transfer_exit(&request, &response);
      break;
    }
    case UDS_REQUEST_FILE_TRANSFER:
    {
      RequestFileTransfer_t request = {0};
      if (!DecodeFileRequest(data, length, &request)) { InvalidLength(sid); return; }
      uds_request_file_transfer(&request);
      break;
    }
    default:
    {
      const uint8_t response[] = { UDS_NEGATIVE_RESPONSE, sid, NRC_SERVICE_NOT_SUPPORTED };
      send_can_message(response, sizeof(response));
      break;
    }
  }
}
