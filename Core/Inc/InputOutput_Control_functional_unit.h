#ifndef INPUT_OUTPUT_CONTROL_FUNCTIONAL_UNIT_H_
#define INPUT_OUTPUT_CONTROL_FUNCTIONAL_UNIT_H_

#include <stdint.h>
#include <stdbool.h>
#include "uds_services.h"

// Maximum data size for input/output control
#define MAX_IO_CONTROL_DATA_SIZE 8

// UDS Service identifiers
#define UDS_INPUT_OUTPUT_CONTROL_BY_IDENTIFIER 0x2F
#define UDS_RESPONSE_INPUT_OUTPUT_CONTROL 0x6F

// Input/output control parameters
#define IOCP_SHORT_TERM_ADJUSTMENT 0x03
#define IOCP_RETURN_CONTROL_TO_ECU 0x00
#define IOCP_FREEZE_CURRENT_STATE 0x02

// Structure for InputOutputControlByIdentifier request
typedef struct {
    uint8_t SID;                  // Service identifier
    uint16_t dataIdentifier;       // Data identifier (DID)
    uint8_t controlOptionRecord;   // Control option (IOCP)
    uint8_t controlState[MAX_IO_CONTROL_DATA_SIZE];  // Control state (maximum 8 bytes)
} IOControlRequest_t;

// Structure for InputOutputControlByIdentifier response
typedef struct {
    uint8_t SID;                   // Service identifier de réponse
    uint16_t dataIdentifier;        // Data identifier (DID)
    uint8_t controlStatusRecord[MAX_IO_CONTROL_DATA_SIZE];  // Control status (maximum 8 bytes)
} IOControlResponse_t;

// Input/output control function prototypes
void uds_input_output_control_by_identifier(IOControlRequest_t *request, IOControlResponse_t *response);
void send_positive_response_input_output_control_by_identifier(uint16_t dataIdentifier, uint8_t controlStatus, uint8_t *controlState);
void send_negative_response_input_output_control_by_identifier(uint8_t nrc);
void handle_input_output_control_response(IOControlResponse_t *response);


#endif /* INPUT_OUTPUT_CONTROL_FUNCTIONAL_UNIT_H_ */
