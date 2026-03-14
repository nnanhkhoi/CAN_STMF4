/*
 * Routine_functional_unit.h
 *
 *  Created on: 21 oct. 2024
 *      Author: PC
 */

#ifndef ROUTINE_FUNCTIONAL_UNIT_C_
#define ROUTINE_FUNCTIONAL_UNIT_C_
#include "uds_services.h"

#include <stdint.h>
#include <stdbool.h>


// RoutineControl service identifier
#define UDS_ROUTINE_CONTROL 0x31
#define UDS_RESPONSE_ROUTINE_CONTROL     0x71

// Sub-functions for RoutineControl
#define ROUTINE_CONTROL_START 0x01
#define ROUTINE_CONTROL_STOP 0x02
#define ROUTINE_CONTROL_REQUEST_RESULTS 0x03



// Structure for RoutineControl request
typedef struct {
    uint8_t subFunction; // Sub-function (start, stop, request results)
    uint16_t routineIdentifier; // Routine identifier (2 bytes)
    uint8_t routineControlOption; // Optional parameters for routine
} RoutineControlRequest_t;

// Structure for RoutineControl response
typedef struct {
    uint8_t SID;                // Response service identifier
    uint8_t routineControlType; // Routine control type
    uint16_t routineIdentifier;  // Routine identifier
    uint8_t routineInfo;        // Routine information
    uint8_t routineStatusRecord[8]; // Routine status
} RoutineControlResponse_t;

void uds_routine_control(RoutineControlRequest_t *request, RoutineControlResponse_t *response);
void send_positive_response_routine_control(RoutineControlResponse_t *response);
void send_negative_response_routine_control(uint8_t nrc);
bool is_routine_supported(uint16_t routineIdentifier);
bool is_security_granted_for_routine(uint16_t routineIdentifier);
bool is_sub_function_supported(uint8_t subFunction);
bool validate_routine_control_option(uint8_t option);



#endif /* ROUTINE_FUNCTIONAL_UNIT_C_ */
