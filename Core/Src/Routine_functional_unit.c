/*
 * Routine_functional_unit.c
 *
 *  Created on: 21 oct. 2024
 *      Author: PC
 */
#include "Routine_functional_unit.h"
#include "uds_services.c"


/*******************************************************RoutineControl******************************************************/
// Function to handle the RoutineControl command
void uds_routine_control(RoutineControlRequest_t *request, RoutineControlResponse_t *response) {
    // 1. Check the minimum message length
    if (sizeof(*request) < (sizeof(request->subFunction) + sizeof(request->routineIdentifier))) {
        send_negative_response_routine_control(NRC_INCORRECT_MESSAGE_LENGTH);
        return;
    }

    // 2. Check the Routine Identifier (RID)
    if (!is_routine_supported(request->routineIdentifier)) {
        send_negative_response_routine_control(NRC_REQUEST_OUT_OF_RANGE);
        return;
    }

    // 3. Check the security for the RID
    if (!is_security_granted_for_routine(request->routineIdentifier)) {
        send_negative_response_routine_control(NRC_SECURITY_ACCESS_DENIED);
        return;
    }

    // 4. Check the Sub-Function
    if (!is_sub_function_supported(request->subFunction)) {
        send_negative_response_routine_control(NRC_SUB_FUNCTION_NOT_SUPPORTED);
        return;
    }

    // 5. Check the total length
    // (Add here if necessary, depending on additional data)

    // 6. Validate the data in routineControlOptionRecord
    if (!validate_routine_control_option(request->routineControlOption))  {
        send_negative_response_routine_control(NRC_REQUEST_OUT_OF_RANGE);
        return;
    }

    // 7. Additional checks (specific conditions)
    // (Add here if necessary)

    // Sub-function processing
    switch (request->subFunction) {

        case 0x01: // Start Routine
            // Logic to start the routine
            // start_routine(request->routineIdentifier);
            break;

        case 0x02: // Stop Routine
            // Logic to stop the routine
            // stop_routine(request->routineIdentifier);
            break;

        case 0x03: // Request Routine Results
            // Logic to request the results of the routine
            // request_routine_results(request->routineIdentifier);
            break;

        default:
            send_negative_response_routine_control(NRC_SUB_FUNCTION_NOT_SUPPORTED);
            return;
    }

    // Prepare the positive response
    response->SID = UDS_RESPONSE_ROUTINE_CONTROL;
    response->routineControlType = request->subFunction;
    response->routineIdentifier = request->routineIdentifier;
    // Fill routineInfo and routineStatusRecord according to the routine logic
    // response->routineInfo = get_routine_info(request->routineIdentifier);

    // Send the positive response
    send_positive_response_routine_control(response);
}


// Function to send a positive response
void send_positive_response_routine_control(RoutineControlResponse_t *response) {
    // Send the message via CAN
    send_can_message((uint8_t *)response, sizeof(RoutineControlResponse_t));
    // send_uart_message((uint8_t *)response, sizeof(RoutineControlResponse_t));
}

// Function to send a negative response
void send_negative_response_routine_control(uint8_t nrc) {
    uint8_t response[2];
    response[0] = UDS_RESPONSE_ROUTINE_CONTROL; // Response SID
    response[1] = nrc; // NRC

    send_can_message(response, sizeof(response));
    // send_uart_message(response, sizeof(response));
}

// Function to check if the routineIdentifier is supported
bool is_routine_supported(uint16_t routineIdentifier) {
    // Implement your logic to check if the routineIdentifier is supported
    return true; // Replace this with the appropriate logic
}

// Function to check security for the routineIdentifier
bool is_security_granted_for_routine(uint16_t routineIdentifier) {
    // Implement your logic to check if security is granted for this routineIdentifier
    return true; // Replace this with the appropriate logic
}

// Function to check if the sub-function is supported
bool is_sub_function_supported(uint8_t subFunction) {
    // Implement your logic to check if the sub-function is supported
    return true; // Replace this with the appropriate logic
}

// Function to validate the data in routineControlOptionRecord
bool validate_routine_control_option(uint8_t option) {
    // Implement your logic to validate the option
    return true; // Modify according to your logic
}
