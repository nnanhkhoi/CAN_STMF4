#ifndef UPLOAD_DOWNLOAD_FUNCTIONAL_UNIT_H_
#define UPLOAD_DOWNLOAD_FUNCTIONAL_UNIT_H_

#include <stdint.h>
#include <stdbool.h>
#include "uds_services.h"

// UDS Upload/Download service identifiers
#define UDS_REQUEST_DOWNLOAD         0x34
#define UDS_REQUEST_UPLOAD           0x35
#define UDS_TRANSFER_DATA            0x36
#define UDS_REQUEST_TRANSFER_EXIT    0x37
#define UDS_REQUEST_FILE_TRANSFER    0x38
#define UDS_RESPONSE_REQUEST_DOWNLOAD    0x74
#define UDS_RESPONSE_TRANSFER_EXIT       0x77



typedef struct {
    uint8_t dataFormatIdentifier;                // Data format identifier
    uint8_t addressAndLengthFormatIdentifier;    // Address and length format identifier
    uint8_t memoryAddress[4];                     // Memory address (max 4 bytes)
    uint8_t memorySize[4];                        // Memory size (max 4 bytes)
} RequestDownload_t;

// Structure for download response
typedef struct {
    uint8_t lengthFormatIdentifier;               // Length format identifier
    uint8_t maxNumberOfBlockLength[2];           // Maximum block length
} ResponseDownload_t;

// Structure definitions
typedef struct {
    uint8_t dataFormatIdentifier; // DFI
    uint8_t addressAndLengthFormatIdentifier; // ALFID
    uint8_t memoryAddress[4]; // Can be adjusted based on size
    uint8_t memorySize[4]; // Can be adjusted based on size
} RequestUpload_t;

typedef struct {
    uint8_t lengthFormatIdentifier; // LFID
    uint8_t maxNumberOfBlockLength[2]; // Can be adjusted based on size
} ResponseUpload_t;

typedef struct {
    uint8_t blockSequenceCounter; // BSC
    uint8_t transferRequestParameterRecord[255]; // Request parameters (adjust as needed)
} RequestTransferData_t;

typedef struct {
    uint8_t blockSequenceCounter; // BSC
    uint8_t transferResponseParameterRecord[255]; // Response parameters (adjust as needed)
} ResponseTransferData_t;

// Structure for RequestTransferExit request
typedef struct {
    uint8_t transferRequestParameterRecord[256]; // Arbitrary size, adjust as needed
} RequestTransferExit_t;

// Structure for service response
typedef struct {
    uint8_t transferResponseParameterRecord[256]; // Arbitrary size, adjust as needed
} ResponseTransferExit_t;

typedef struct {
    uint8_t modeOfOperation;             // 0x01 - 0x05 : Operation mode
    uint16_t filePathAndNameLength;      // File path length
    uint8_t filePathAndName[255];        // File path (up to 255 bytes)
    uint8_t dataFormatIdentifier;         // Data format identifier
    uint16_t fileSizeParameterLength;     // File size parameter length
    uint8_t fileSizeUncompressed[4];      // Uncompressed size (4 bytes)
    uint8_t fileSizeCompressed[4];        // Compressed size (4 bytes)
} RequestFileTransfer_t;


typedef struct {
    uint8_t SID; // Service identifier
    uint8_t modeOfOperation; // Operation mode echo
    uint8_t lengthFormatIdentifier; // maxNumberOfBlockLength length
    uint8_t maxNumberOfBlockLength[2]; // Maximum block length
    uint8_t dataFormatIdentifier; // Data format echo
    uint16_t fileSizeOrDirInfoParameterLength; // Parameter length
    uint8_t fileSizeUncompressedOrDirInfoLength[4]; // File size
    uint8_t fileSizeCompressed[4]; // Compressed size
} ResponseFileTransfer_t;


void uds_request_download(RequestDownload_t *request);
void send_positive_response_request_download(ResponseDownload_t *response);
void send_negative_response_request_download(uint8_t nrc);
bool is_memory_address_valid(uint8_t *address);
bool is_memory_size_valid(uint8_t *size);
bool is_security_active(void);

void uds_request_upload(RequestUpload_t *request);
void send_positive_response_upload(uint8_t *maxNumberOfBlockLength);
void send_negative_response_upload(uint8_t nrc);
bool is_security_active(void);
bool validate_request_upload(RequestUpload_t *request);

void uds_transfer_data(RequestTransferData_t *request);
void send_positive_response_transfer_data(uint8_t blockSequenceCounter);
void send_negative_response_transfer_data(uint8_t nrc);
bool is_request_download_active(void);
bool is_request_upload_active(void);
bool validate_transfer_data_request(RequestTransferData_t *request);

void uds_request_transfer_exit(RequestTransferExit_t *request, ResponseTransferExit_t *response);
void send_positive_response_transfer_exit(ResponseTransferExit_t *response);
void send_negative_response_transfer_exit(uint8_t nrc);
bool is_transfer_in_progress(void);
bool validate_transfer_request_parameters(RequestTransferExit_t *request);

void uds_request_file_transfer(RequestFileTransfer_t *request);
void send_positive_response_file_transfer(ResponseFileTransfer_t *response);
void send_negative_response_file_transfer(uint8_t nrc);

void uds_request_download(RequestDownload_t *request);
void send_positive_response_request_download(ResponseDownload_t *response);
void send_negative_response_request_download(uint8_t nrc);
bool is_memory_address_valid(uint8_t *address);
bool is_memory_size_valid(uint8_t *size);
bool is_security_active(void);

void uds_request_upload(RequestUpload_t *request);
void send_positive_response_upload(uint8_t *maxNumberOfBlockLength);
void send_negative_response_upload(uint8_t nrc);
bool is_security_active(void);
bool validate_request_upload(RequestUpload_t *request);

void uds_transfer_data(RequestTransferData_t *request);
void send_positive_response_transfer_data(uint8_t blockSequenceCounter);
void send_negative_response_transfer_data(uint8_t nrc);
bool is_request_download_active(void);
bool is_request_upload_active(void);
bool validate_transfer_data_request(RequestTransferData_t *request);

void uds_request_transfer_exit(RequestTransferExit_t *request, ResponseTransferExit_t *response);
void send_positive_response_transfer_exit(ResponseTransferExit_t *response);
void send_negative_response_transfer_exit(uint8_t nrc);
bool is_transfer_in_progress(void);
bool validate_transfer_request_parameters(RequestTransferExit_t *request);

void uds_request_file_transfer(RequestFileTransfer_t *request);
void send_positive_response_file_transfer(ResponseFileTransfer_t *response);
void send_negative_response_file_transfer(uint8_t nrc);

#endif /* UPLOAD_DOWNLOAD_FUNCTIONAL_UNIT_H_ */
