/*
 * Upload_Download_functional_unit.c
 *
 * Created on: Oct 21, 2024
 * Author: PC
 */
#include "uds_services.c"
#include "Upload_Download_functional_unit.h"

/*******************************************************Request_download******************************************************/
// Function to process the download request
void uds_request_download(RequestDownload_t *request) {
    // 1. Minimum length verification
    if (sizeof(*request) < sizeof(request->dataFormatIdentifier) + sizeof(request->addressAndLengthFormatIdentifier) + 4) {
        send_negative_response_request_download(NRC_INCORRECT_MESSAGE_LENGTH);
        return;
    }

    // 2. Data format identifier verification
    if (request->dataFormatIdentifier != 0x00) { // Verification example
        send_negative_response_request_download(NRC_REQUEST_OUT_OF_RANGE);
        return;
    }

    // 3. Address and length format identifier verification
    if (!is_memory_address_valid(request->memoryAddress) || !is_memory_size_valid(request->memorySize)) {
        send_negative_response_request_download(NRC_REQUEST_OUT_OF_RANGE);
        return;
    }

    // 4. Security verification (if applicable)
    if (is_security_active()) { // Implement is_security_active()
        send_negative_response_request_download(NRC_SECURITY_ACCESS_DENIED);
        return;
    }

    // Prepare positive response
    ResponseDownload_t response;
    response.lengthFormatIdentifier = 0x74; // Response identifier
    response.maxNumberOfBlockLength[0] = 0x00; // Replace with appropriate logic
    response.maxNumberOfBlockLength[1] = 0xFF; // Replace with appropriate logic

    // Send positive response
    send_positive_response_request_download(&response);
}

// Function to send a positive response
void send_positive_response_request_download(ResponseDownload_t *response) {
    // Sending message via CAN
    send_can_message((uint8_t *)response, sizeof(ResponseDownload_t));
    // send_uart_message((uint8_t *)response, sizeof(ResponseDownload_t));
}

// Function to send a negative response
void send_negative_response_request_download(uint8_t nrc) {
    uint8_t response[2];
    response[0] = UDS_RESPONSE_REQUEST_DOWNLOAD; // Response SID
    response[1] = nrc; // NRC
    send_can_message(response, sizeof(response));
    //send_uart_message(response, sizeof(response));
}

// Function to verify memory address validity
bool is_memory_address_valid(uint8_t *address) {
    // Implement your logic to verify address validity
    return true; // Replace with appropriate logic
}

// Function to verify memory size validity
bool is_memory_size_valid(uint8_t *size) {
    // Implement your logic to verify size validity
    return true; // Replace with appropriate logic
}

/*******************************************************RequestUpload******************************************************/

// Function to handle the RequestUpload query
void uds_request_upload(RequestUpload_t *request) {
    // 1. Message length verification
    if (sizeof(*request) < sizeof(request->dataFormatIdentifier) + sizeof(request->addressAndLengthFormatIdentifier)) {
        send_negative_response_upload(NRC_INCORRECT_MESSAGE_LENGTH);
        return;
    }

    // 2. Identifier and data format verification
    if (request->dataFormatIdentifier != 0x35) { // Validation example
        send_negative_response_upload(NRC_REQUEST_OUT_OF_RANGE);
        return;
    }

    // 3. Security verification
    if (is_security_active()) {
        send_negative_response_upload(NRC_SECURITY_ACCESS_DENIED);
        return;
    }

    // 4. Data validation in memoryAddress and memorySize
    // Implement your logic to check if the address and size are valid

    // 5. Send a positive response
    uint8_t maxNumberOfBlockLength[2] = {0x00, 0xFF}; // Example
    send_positive_response_upload(maxNumberOfBlockLength);
}

// Function to send a positive response
void send_positive_response_upload(uint8_t *maxNumberOfBlockLength) {
    ResponseUpload_t response;
    response.lengthFormatIdentifier = 0x75; // LFID
    response.maxNumberOfBlockLength[0] = maxNumberOfBlockLength[0];
    response.maxNumberOfBlockLength[1] = maxNumberOfBlockLength[1];

    // Send the message via CAN protocol or other appropriate method
    send_can_message((uint8_t*)&response, sizeof(response));
    //send_uart_message((uint8_t*)&response, sizeof(response));
}

// Function to send a negative response
void send_negative_response_upload(uint8_t nrc) {
    uint8_t response[2];
    response[0] = 0x35; // Response SID
    response[1] = nrc;

    // Send the error message
    send_can_message(response, sizeof(response));
    //send_uart_message(response, sizeof(response));
}

// Example function to check if the security level is active
bool is_security_active(void) {
    // Logic to check if security is active
    return false; // Replace with your logic
}

// Function to validate the request
bool validate_request_upload(RequestUpload_t *request) {
    // Implement validation logic
    return true; // Replace with your logic
}

/*******************************************************TransferData******************************************************/
#include "uds_services.h"

// Function to handle the TransferData request
void uds_transfer_data(RequestTransferData_t *request) {
    // 1. Message length verification
    if (sizeof(*request) < sizeof(request->blockSequenceCounter)) {
        send_negative_response_transfer_data(NRC_INCORRECT_MESSAGE_LENGTH);
        return;
    }

    // 2. Verification if a download or upload request is active
    if (!is_request_download_active() && !is_request_upload_active()) {
        send_negative_response_transfer_data(NRC_REQUEST_SEQUENCE_ERROR);
        return;
    }

    // 3. Verification of blockSequenceCounter validity
    static uint8_t lastBlockSequenceCounter = 0;
    if (request->blockSequenceCounter != (lastBlockSequenceCounter + 1) % 256) {
        send_negative_response_transfer_data(NRC_WRONG_BLOCK_SEQUENCE_COUNTER);
        return;
    }

    // 4. Processing of transferred data
    // If it's a download, write data to memory
    // If it's an upload, read data from memory
    // Example (to be adapted to your needs):
    // write_data_to_memory(request->transferRequestParameterRecord);

    // 5. Update the last used blockSequenceCounter
    lastBlockSequenceCounter = request->blockSequenceCounter;

    // 6. Send a positive response
    send_positive_response_transfer_data(request->blockSequenceCounter);
}

// Function to send a positive response
#include <string.h> // Include for memcpy

void send_positive_response_transfer_data(uint8_t blockSequenceCounter) {
    ResponseTransferData_t response;
    response.blockSequenceCounter = blockSequenceCounter;

    // Fill response.transferResponseParameterRecord if necessary
    // Example:
    // response.transferResponseParameterRecord[0] = ...;

    // Calculate total size of message to send
    size_t responseSize = sizeof(response.blockSequenceCounter) +
                          sizeof(response.transferResponseParameterRecord); // Add size of all members

    // Send message via CAN protocol or other appropriate method
    send_can_message((uint8_t*)&response, responseSize);
    // send_uart_message((uint8_t*)&response, responseSize);
}

// Function to send a negative response
void send_negative_response_transfer_data(uint8_t nrc) {
    uint8_t response[2];
    response[0] = 0x36; // Response SID
    response[1] = nrc;

    // Send error message
    send_can_message(response, sizeof(response));
    //send_uart_message(response, sizeof(response));
}

// Example of request activity verification
bool is_request_download_active(void) {
    // Logic to check if a download request is active
    return false; // Replace with your logic
}

bool is_request_upload_active(void) {
    // Logic to check if an upload request is active
    return false; // Replace with your logic
}

// Function to validate the data transfer request
bool validate_transfer_data_request(RequestTransferData_t *request) {
    // Implement validation logic
    return true; // Replace with your logic
}

/*******************************************************RequestTransferExit*************************************************/
// Function to handle the RequestTransferExit request
void uds_request_transfer_exit(RequestTransferExit_t *request, ResponseTransferExit_t *response) {
    // 1. Message length verification
    if (sizeof(*request) < sizeof(request->transferRequestParameterRecord)) {
        send_negative_response_transfer_exit(NRC_INCORRECT_MESSAGE_LENGTH);
        return;
    }

    // 2. Verification of request sequence state
    if (!is_transfer_in_progress()) {
        send_negative_response_transfer_exit(NRC_REQUEST_SEQUENCE_ERROR);
        return;
    }

    // 3. Validation of data in transferRequestParameterRecord
    if (!validate_transfer_request_parameters(request)) {
        send_negative_response_transfer_exit(NRC_REQUEST_OUT_OF_RANGE);
        return;
    }

    // 4. Logic to finalize the transfer request
    // (Add necessary logic here to finalize the data transfer)

    // 5. Preparation of positive response
    memset(response, 0, sizeof(ResponseTransferExit_t)); // Reset response
    // Fill response.transferResponseParameterRecord if necessary
    // Example: response.transferResponseParameterRecord[0] = ...;

    // Send positive response
    send_positive_response_transfer_exit(response);
}

// Function to send a positive response
void send_positive_response_transfer_exit(ResponseTransferExit_t *response) {
    // Send message via CAN protocol or other appropriate method
    size_t response_size = sizeof(ResponseTransferExit_t); // Response size
    send_can_message((uint8_t*)response, response_size);
}

// Function to send a negative response
void send_negative_response_transfer_exit(uint8_t nrc) {
    // Sending an error message via CAN
    uint8_t response[2];
    response[0] = UDS_RESPONSE_TRANSFER_EXIT; // Response SID
    response[1] = nrc; // NRC

    send_can_message(response, sizeof(response));
    //send_uart_message(response, sizeof(response));
}

// Function to check if a transfer is in progress
bool is_transfer_in_progress(void) {
    // Implement your logic to check if a transfer is active
    return true; // Replace this with appropriate logic
}

// Function to validate transfer request parameters
bool validate_transfer_request_parameters(RequestTransferExit_t *request) {
    // Implement your logic to validate parameters
    return true; // Replace this with appropriate logic
}

/*******************************************************RequestFileTransfer*************************************************/
// Function to process the RequestFileTransfer request
void uds_request_file_transfer(RequestFileTransfer_t *request) {
    // 1. Message length verification
    if (request->filePathAndNameLength > sizeof(request->filePathAndName)) {
        send_negative_response_file_transfer(0x13); // Incorrect length
        return;
    }

    // 2. Operation mode verification
    if (request->modeOfOperation < 0x01 || request->modeOfOperation > 0x05) {
        send_negative_response_file_transfer(0x31); // Mode out of range
        return;
    }

    // 3. Additional verifications based on operation mode
    // (Add specific verifications here if necessary)

    // 4. Send positive response
    ResponseFileTransfer_t response;
    response.SID = 0x78; // Response identifier
    response.modeOfOperation = request->modeOfOperation;
    response.lengthFormatIdentifier = 0x00; // To be defined according to context
    response.maxNumberOfBlockLength[0] = 0xFF; // Example max length
    response.maxNumberOfBlockLength[1] = 0xFF; // Example max length
    response.dataFormatIdentifier = request->dataFormatIdentifier;

    // Fill file lengths if necessary
    // Example: response.fileSizeOrDirInfoParameterLength = ...;

    // Call function to send positive response
    send_positive_response_file_transfer(&response);
}

// Function to send a positive response
void send_positive_response_file_transfer(ResponseFileTransfer_t *response) {
    // Send message via CAN protocol or other appropriate method
    send_can_message((uint8_t*)response, sizeof(ResponseFileTransfer_t));
    //send_uart_message((uint8_t*)response, sizeof(ResponseFileTransfer_t));
}

// Function to send a negative response
void send_negative_response_file_transfer(uint8_t nrc) {
    uint8_t response[2];
    response[0] = 0x78; // Response identifier
    response[1] = nrc; // NRC

    // Send an error message via CAN
    send_can_message(response, sizeof(response));
}