/*
 * InputOutput_Control_functional_unit.c
 *
 *  Created on: 21 oct. 2024
 *      Author: PC
 */
#include "InputOutput_Control_functional_unit.h"
#include "uds_services.h"
#include <string.h>


/************************************************InputOutputControlByIdentifier*************************************************/

// Function to handle the InputOutputControlByIdentifier command
void uds_input_output_control_by_identifier(IOControlRequest_t *request, IOControlResponse_t *response) {
    // Check the minimum message length
    if (sizeof(*request) < (sizeof(request->SID) + sizeof(request->dataIdentifier) + sizeof(request->controlOptionRecord))) {
        send_negative_response_input_output_control_by_identifier(NRC_INCORRECT_MESSAGE_LENGTH);
        return;
    }

    // Check if the Data Identifier (DID) is supported for writing
    if (!is_data_identifier_supported_for_write(request->dataIdentifier)) {
        send_negative_response_input_output_control_by_identifier(NRC_REQUEST_OUT_OF_RANGE);
        return;
    }

    // Process control options
    switch (request->controlOptionRecord) {
        case IOCP_SHORT_TERM_ADJUSTMENT:
            // Apply control states
            // Here, you should add your logic to control the ECU
            // Example: write_data_to_identifier(request->dataIdentifier, request->controlState, sizeof(request->controlState));
            break;

        case IOCP_RETURN_CONTROL_TO_ECU:
            // Logic to return control to the ECU
            // Example: return_control_to_ecu(request->dataIdentifier);
            break;

        case IOCP_FREEZE_CURRENT_STATE:
            // Logic to freeze the current state
            // Example: freeze_current_state(request->dataIdentifier);
            break;

        default:
            send_negative_response_input_output_control_by_identifier(NRC_CONDITIONS_NOT_CORRECT);
            return;
    }

    // Prepare the positive response
    response->SID = UDS_RESPONSE_INPUT_OUTPUT_CONTROL;
    response->dataIdentifier = request->dataIdentifier;

    // Fill the control status array with the current or desired values
    memcpy(response->controlStatusRecord, request->controlState, sizeof(request->controlState));

    // Send the positive response
    send_positive_response_input_output_control_by_identifier(request->dataIdentifier, request->controlOptionRecord, response->controlStatusRecord);
}

// Function to send a positive response
void send_positive_response_input_output_control_by_identifier(uint16_t dataIdentifier, uint8_t controlOptionRecord, uint8_t *controlStatusRecord) {
    IOControlResponse_t response;

    response.SID = UDS_RESPONSE_INPUT_OUTPUT_CONTROL;
    response.dataIdentifier = dataIdentifier;

    // Copy control states into the response
    memcpy(response.controlStatusRecord, controlStatusRecord, sizeof(response.controlStatusRecord));

    // Send the message via CAN
    send_can_message((uint8_t*)&response, sizeof(response));
    // send_uart_message((uint8_t*)&response, sizeof(response));
}


// Function to send a negative response
void send_negative_response_input_output_control_by_identifier(uint8_t nrc) {
    // Send an error message via CAN
    // Example: send_can_message(&nrc, sizeof(nrc));
    uint8_t response[2];
    response[0] = UDS_RESPONSE_INPUT_OUTPUT_CONTROL; // Response SID
    response[1] = nrc; // NRC

    send_can_message(response, sizeof(response));
    // send_uart_message(response, sizeof(response));
}

// Function to handle the response to the service
void handle_input_output_control_response(IOControlResponse_t *response) {
    // Logic to process the response if necessary
    // Example: display the received control state
}



