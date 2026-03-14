/*
 * Stored_Data_Transmission_functional_unit.c
 *
 *  Created on: 21 oct. 2024
 *      Author: PC
 */
#include "Stored_Data_Transmission_functional_unit.h"
#include "uds_services.c"



/************************************************ClearDiagnosticInformation*************************************************/
// Main function for ClearDiagnosticInformation
void uds_clear_diagnostic_information(uint8_t* data, uint8_t data_length) {
    // Check the message length (SID + groupOfDTC = 4 bytes)
    if (data_length != 4) {
        send_negative_response_clear_diagnostic_information(NRC_INCORRECT_MESSAGE_LENGTH);
        return;
    }

    // Extract groupOfDTC (3 bytes)
    uint32_t groupOfDTC = (data[1] << 16) | (data[2] << 8) | data[3];

    // Check if groupOfDTC is supported
    if (!is_group_of_dtc_supported(groupOfDTC)) {
        send_negative_response_clear_diagnostic_information(NRC_REQUEST_OUT_OF_RANGE);
        return;
    }

    // Check if the conditions for clearing DTCs are correct
    if (!are_conditions_correct_for_dtc_clear()) {
        send_negative_response_clear_diagnostic_information(NRC_CONDITIONS_NOT_CORRECT);
        return;
    }

    // Clear diagnostic information for the groupOfDTC
    if (!clear_diagnostic_information(groupOfDTC)) {
        send_negative_response_clear_diagnostic_information(NRC_GENERAL_PROGRAMMING_FAILURE);
        return;
    }

    // If everything is correct, send a positive response
    send_positive_response_clear_diagnostic_information();
}

// Check if groupOfDTC is supported
bool is_group_of_dtc_supported(uint32_t groupOfDTC) {
    // Implement logic to check if groupOfDTC is supported
    // For example, suppose we only support certain DTC groups
    switch (groupOfDTC) {
        case 0x000000:  // Example DTC groups
        case 0x010000:  // Powertrain group
        case 0x020000:  // Chassis group
        case 0x030000:  // Body group
            return true;
        default:
            return false;
    }
}

// Check if the conditions for clearing DTCs are correct
bool are_conditions_correct_for_dtc_clear(void) {
    // Implement specific checks, for example:
    // - Is the vehicle in a mode that allows DTC clearing?
    // - Is the current diagnostic session correct?

    if (uds_session.current_session == UDS_SESSION_DEFAULT) {
        return false;  // Clearing is not allowed in the default session
    }
    // Add other checks as needed (engine state, etc.)

    return true;  // All conditions are correct
}

// Clear diagnostic information for the specified groupOfDTC
bool clear_diagnostic_information(uint32_t groupOfDTC) {
    // Implement logic to clear DTC information
    // Here, we simply simulate clearing, but in a real system,
    // this would include memory operations and DTC database handling.

    // Example: Suppose clearing always succeeds for this demonstration
    return true;

    // In case of failure (e.g., memory write failure), return false
}

// Send a positive response after successful clearing
void send_positive_response_clear_diagnostic_information(void) {
    uint8_t response[1];
    response[0] = UDS_CLEAR_DIAGNOSTIC_INFORMATION + 0x40;  // Positive response SID (0x14 + 0x40 = 0x54)
    send_can_message(response, 1);  // Send the response on the CAN bus
    // send_uart_message(response, 1);
}

// Send a negative response in case of error
void send_negative_response_clear_diagnostic_information(uint8_t nrc) {
    uint8_t response[3];
    response[0] = UDS_NEGATIVE_RESPONSE;  // SID for a negative response
    response[1] = UDS_CLEAR_DIAGNOSTIC_INFORMATION;  // SID of ClearDiagnosticInformation (0x14)
    response[2] = nrc;  // Negative response code (NRC)
    send_can_message(response, 3);  // Send the response on the CAN bus
    // send_uart_message(response, 3);
}


/************************************************read_dtc_information*******************************************************/
DTC_Record stored_dtc_list[MAX_DTC_COUNT];  // List of stored DTCs
DTC_Record mirror_dtc_list[MAX_DTC_COUNT];
DTC_Record user_defined_memory_list[MAX_DTC_COUNT];


// Main function of the ReadDTCInformation service
void uds_read_dtc_information(uint8_t sub_function, uint8_t* data, uint8_t data_length) {
    switch (sub_function) {
        case REPORT_NUMBER_OF_DTC_BY_STATUS_MASK:
            report_number_of_dtc_by_status_mask(data, data_length);
            break;
        case REPORT_DTC_BY_STATUS_MASK:
            report_dtc_by_status_mask(data, data_length);
            break;
        case REPORT_DTC_SNAPSHOT_IDENTIFICATION:
            report_dtc_snapshot_identification(data, data_length);
            break;
        case REPORT_DTC_SNAPSHOT_RECORD_BY_DTC_NUMBER:
            report_dtc_snapshot_record_by_dtc_number(data, data_length);
            break;
        case REPORT_DTC_STORED_DATA_BY_RECORD_NUMBER:
            report_dtc_stored_data_by_record_number(data, data_length);
            break;
        case REPORT_DTC_EXT_DATA_RECORD_BY_DTC_NUMBER:
            report_dtc_ext_data_record_by_dtc_number(data, data_length);
            break;
        case REPORT_NUMBER_OF_DTC_BY_SEVERITY_MASK:
            report_number_of_dtc_by_severity_mask(data, data_length);
            break;
        case REPORT_DTC_BY_SEVERITY_MASK_RECORD:
            report_dtc_by_severity_mask_record(data, data_length);
            break;
        case REPORT_SEVERITY_INFORMATION_OF_DTC:
            report_severity_information_of_dtc(data, data_length);
            break;
        case REPORT_SUPPORTED_DTC:
            report_supported_dtc(data, data_length);
            break;
        case REPORT_FIRST_TEST_FAILED_DTC:
            report_first_test_failed_dtc(data, data_length);
            break;
        case REPORT_FIRST_CONFIRMED_DTC:
            report_first_confirmed_dtc(data, data_length);
            break;
        case REPORT_MOST_RECENT_TEST_FAILED_DTC:
            report_most_recent_test_failed_dtc(data, data_length);
            break;
        case REPORT_MOST_RECENT_CONFIRMED_DTC:
            report_most_recent_confirmed_dtc(data, data_length);
            break;
        case REPORT_MIRROR_MEMORY_DTC_BY_STATUS_MASK:
            report_mirror_memory_dtc_by_status_mask(data, data_length);
            break;
        case REPORT_MIRROR_MEMORY_DTC_EXT_DATA_RECORD:
            report_mirror_memory_dtc_ext_data_record(data, data_length);
            break;
        case REPORT_NUMBER_OF_MIRROR_MEMORY_DTC_BY_STATUS_MASK:
            report_number_of_mirror_memory_dtc_by_status_mask(data, data_length);
            break;
        case REPORT_NUMBER_OF_EMISSIONS_OBD_DTC_BY_STATUS_MASK:
            report_number_of_emissions_obd_dtc_by_status_mask(data, data_length);
            break;
        case REPORT_EMISSIONS_OBD_DTC_BY_STATUS_MASK:
            report_emissions_obd_dtc_by_status_mask(data, data_length);
            break;
        case REPORT_DTC_FAULT_DETECTION_COUNTER:
            report_dtc_fault_detection_counter(data, data_length);
            break;
        case REPORT_DTC_WITH_PERMANENT_STATUS:
            report_dtc_with_permanent_status(data, data_length);
            break;
        case REPORT_DTC_EXT_DATA_RECORD_BY_RECORD_NUMBER:
            report_dtc_ext_data_record_by_record_number(data, data_length);
            break;
        case REPORT_USER_DEF_MEMORY_DTC_BY_STATUS_MASK:
            report_user_def_memory_dtc_by_status_mask(data, data_length);
            break;
        case REPORT_USER_DEF_MEMORY_DTC_SNAPSHOT_RECORD:
            report_user_def_memory_dtc_snapshot_record(data, data_length);
            break;
        case REPORT_USER_DEF_MEMORY_DTC_EXT_DATA_RECORD:
            report_user_def_memory_dtc_ext_data_record(data, data_length);
            break;
        case REPORT_WWH_OBD_DTC_BY_MASK_RECORD:
            report_wwh_obd_dtc_by_mask_record(data, data_length);
            break;
        case REPORT_WWH_OBD_DTC_WITH_PERMANENT_STATUS:
            report_wwh_obd_dtc_with_permanent_status(data, data_length);
            break;
        default:
            send_negative_response_read_dtc_information(sub_function, NRC_SUB_FUNCTION_NOT_SUPPORTED);
            break;
    }
}

void send_positive_response_read_dtc_information(uint8_t sub_function, DTC_Record* dtcRecords, uint8_t dtcCount) {
    // Limit the maximum size of the response to the size of a CAN message (e.g., 8 bytes)
    uint8_t response[8];  // Array of response data limited to 8 bytes
    uint8_t index = 0;

    // Field 1: SID for ReadDTCInformation
    response[index++] = 0x59;  // SID for ReadDTCInformation (positive response)

    // Field 2: Type of report (sub_function)
    response[index++] = sub_function;

    switch (sub_function) {
        case REPORT_NUMBER_OF_DTC_BY_STATUS_MASK:
        case REPORT_NUMBER_OF_DTC_BY_SEVERITY_MASK:
            // Fields 3: DTCStatusAvailabilityMask
            response[index++] = get_dtc_status_availability_mask();

            // Fields 4: Number of DTCs
            response[index++] = (dtcCount >> 8) & 0xFF;  // Octet high of the DTC count
            response[index++] = dtcCount & 0xFF;         // Octet low of the DTC count
            break;

        case REPORT_DTC_BY_STATUS_MASK:
        case REPORT_SUPPORTED_DTC:
        case REPORT_FIRST_TEST_FAILED_DTC:
            // Add information about each DTC, send in multiple messages if necessary
            for (uint8_t i = 0; i < dtcCount; i++) {
                index = 2;  // Reset index after SID and sub-function for each new message

                // Fields DTC
                response[index++] = (dtcRecords[i].dtcNumber >> 16) & 0xFF;  // Octet high of the DTC
                response[index++] = (dtcRecords[i].dtcNumber >> 8) & 0xFF;   // Octet middle of the DTC
                response[index++] = dtcRecords[i].dtcNumber & 0xFF;          // Octet low of the DTC
                response[index++] = dtcRecords[i].status;                    // Status of the DTC

                // If the response is complete (7 bytes max for a CAN message), send the message
                send_can_message(response, index);

                // Reset the response array for the next DTC
                index = 0;
            }
            return;  // All messages have been sent
            break;

        default:
            send_negative_response_read_dtc_information(sub_function, NRC_SUB_FUNCTION_NOT_SUPPORTED);
            return;
    }

    // Send the message if all bytes fit in a single CAN message
    send_can_message(response, index);
    // send_uart_message(response, 3);
}




// Send a negative response for ReadDTCInformation
void send_negative_response_read_dtc_information(uint8_t sub_function, uint8_t nrc) {
    uint8_t response[3];
    response[0] = 0x7F; // Negative Response SID
    response[1] = sub_function;
    response[2] = nrc;
    send_can_message(response, 3);
    // send_uart_message(response, 3);
}

// Implementation of ReadDTCInformation sub-functions
void report_number_of_dtc_by_status_mask(uint8_t* data, uint8_t data_length) {
    // Check the length of the received data
    if (data_length != 1) {
        send_negative_response_read_dtc_information(REPORT_NUMBER_OF_DTC_BY_STATUS_MASK, NRC_INCORRECT_MESSAGE_LENGTH);
        return;
    }

    uint8_t status_mask = data[0];  // The status mask is sent first
    uint16_t dtc_count = 0;         // Counter for DTCs
    uint8_t dtc_status_availability_mask = get_dtc_status_availability_mask();
    uint8_t dtc_format_identifier = get_dtc_format_identifier();

    // Count the number of DTCs that correspond to the status mask
    for (uint8_t i = 0; i < MAX_DTC_COUNT; i++) {
        if ((stored_dtc_list[i].status & status_mask) != 0) {
            dtc_count++;
        }
    }

    // If no DTCs are found, send an NRC (no conditions met)
    if (dtc_count == 0) {
        send_negative_response_read_dtc_information(REPORT_NUMBER_OF_DTC_BY_STATUS_MASK, NRC_CONDITIONS_NOT_CORRECT);
        return;
    }

    // Dynamically allocate memory for the response (4 bytes)
    uint8_t* response = (uint8_t*)malloc(4 * sizeof(uint8_t));
    if (response == NULL) {
        // If allocation fails, handle the error
        Error_Handler();
        return;
    }

    // Create the response with the availability mask and the format identifier
    response[0] = dtc_status_availability_mask;
    response[1] = dtc_format_identifier;
    response[2] = (dtc_count >> 8) & 0xFF;  // Octet high of the DTC count
    response[3] = dtc_count & 0xFF;         // Octet low of the DTC count

    // Send the response positive to the client via CAN (or another communication protocol)
    send_can_message(response, 4);  // Use send_can_message to send the response as an array of bytes
    // send_uart_message(response, 4);
    // Free the dynamically allocated memory
    free(response);
}

void report_dtc_by_status_mask(uint8_t* data, uint8_t data_length) {
    // Check the length of the received data
    if (data_length != 1) {
        send_negative_response_read_dtc_information(REPORT_DTC_BY_STATUS_MASK, NRC_INCORRECT_MESSAGE_LENGTH);
        return;
    }

    uint8_t status_mask = data[0];  // The status mask is sent first
    uint16_t dtc_count = 0;         // Counter for DTCs

    // Count the number of DTCs corresponding to the status mask
    for (uint8_t i = 0; i < MAX_DTC_COUNT; i++) {
        if ((stored_dtc_list[i].status & status_mask) != 0) {
            dtc_count++;
        }
    }

    // If no DTCs are found, send an NRC (no conditions met)
    if (dtc_count == 0) {
        send_negative_response_read_dtc_information(REPORT_DTC_BY_STATUS_MASK, NRC_CONDITIONS_NOT_CORRECT);
        return;
    }

    // Dynamically allocate memory for the DTC records
    DTC_Record* dtc_records = (DTC_Record*)malloc(dtc_count * sizeof(DTC_Record));
    if (dtc_records == NULL) {
        // If allocation fails, handle the error
        Error_Handler();
        return;
    }

    // Fill the DTC records
    uint16_t index = 0;
    for (uint8_t i = 0; i < MAX_DTC_COUNT; i++) {
        if ((stored_dtc_list[i].status & status_mask) != 0 && index < dtc_count) {
            dtc_records[index].dtcNumber = stored_dtc_list[i].dtcNumber;
            dtc_records[index].status = stored_dtc_list[i].status;
            dtc_records[index].severity = stored_dtc_list[i].severity;
            index++;
        }
    }

    // Send the response positive with the DTC records
    send_positive_response_read_dtc_information(REPORT_DTC_BY_STATUS_MASK, dtc_records, dtc_count);

    // Free the dynamically allocated memory
    free(dtc_records);
}



void report_dtc_snapshot_identification(uint8_t* data, uint8_t data_length) {
    // Check the length of the received data
    if (data_length != 0) {  // No data is expected for this service
        send_negative_response_read_dtc_information(REPORT_DTC_SNAPSHOT_IDENTIFICATION, NRC_INCORRECT_MESSAGE_LENGTH);
        return;
    }

    uint16_t dtc_count = 0;  // Counter for DTCs
    uint16_t snapshot_count = 0;  // Counter for snapshots

    // Count the number of DTCs that have a snapshot
    for (uint8_t i = 0; i < MAX_DTC_COUNT; i++) {
        if (stored_dtc_list[i].snapshotDataLength > 0) {  // A snapshot is present for this DTC
            snapshot_count += stored_dtc_list[i].snapshotRecordNumber;  // Count each occurrence
            dtc_count++;
        }
    }

    // If no snapshot is found, send an NRC
    if (snapshot_count == 0) {
        send_negative_response_read_dtc_information(REPORT_DTC_SNAPSHOT_IDENTIFICATION, NRC_CONDITIONS_NOT_CORRECT);
        return;
    }

    // Dynamically allocate memory for the DTC records
    DTC_Record* dtc_records = (DTC_Record*)malloc(snapshot_count * sizeof(DTC_Record));
    if (dtc_records == NULL) {
        // If allocation fails, handle the error
        Error_Handler();
        return;
    }

    // Fill the DTC records with the DTC number and the snapshot number
    uint16_t index = 0;
    for (uint8_t i = 0; i < MAX_DTC_COUNT; i++) {
        if (stored_dtc_list[i].snapshotDataLength > 0) {
            // For each snapshot occurrence for a DTC
            for (uint8_t j = 0; j < stored_dtc_list[i].snapshotRecordNumber; j++) {
                dtc_records[index].dtcNumber = stored_dtc_list[i].dtcNumber;
                dtc_records[index].snapshotRecordNumber = j + 1;  // Snapshot numbering
                index++;
            }
        }
    }

    // Send the positive response with the DTC records
    send_positive_response_read_dtc_information(REPORT_DTC_SNAPSHOT_IDENTIFICATION, dtc_records, snapshot_count);

    // Free the dynamically allocated memory
    free(dtc_records);
}



void report_dtc_snapshot_record_by_dtc_number(uint8_t* data, uint8_t data_length) {
    // Check the length of the received data (DTCMaskRecord + SnapshotRecordNumber = 4 bytes expected)
    if (data_length != 4) {
        send_negative_response_read_dtc_information(REPORT_DTC_SNAPSHOT_RECORD_BY_DTC_NUMBER, NRC_INCORRECT_MESSAGE_LENGTH);
        return;
    }

    // Extract the DTC number from the client request (3 bytes)
    uint32_t dtc_mask_record = (data[0] << 16) | (data[1] << 8) | data[2];
    uint8_t snapshot_record_number = data[3];  // Number of the snapshot

    uint8_t found = 0;
    DTC_Record dtc_record;  // No dynamic allocation for DTC_Record

    // Browse the list of DTCs to find the DTC corresponding to the mask
    for (uint8_t i = 0; i < MAX_DTC_COUNT; i++) {
        if (stored_dtc_list[i].dtcNumber == dtc_mask_record) {
            // If a snapshot is found
            if ((stored_dtc_list[i].snapshotRecordNumber >= snapshot_record_number && snapshot_record_number != 0xFF) || snapshot_record_number == 0xFF) {
                dtc_record.dtcNumber = stored_dtc_list[i].dtcNumber;
                dtc_record.status = stored_dtc_list[i].status;
                dtc_record.snapshotRecordNumber = snapshot_record_number;

                // Fill the snapshot data
                for (uint8_t j = 0; j < stored_dtc_list[i].snapshotDataLength; j++) {
                    dtc_record.snapshotData[j] = stored_dtc_list[i].snapshotData[j];  // Use the fixed array
                }
                dtc_record.snapshotDataLength = stored_dtc_list[i].snapshotDataLength;
                found = 1;
                break;
            }
        }
    }

    // If no record is found, send an NRC
    if (!found) {
        send_negative_response_read_dtc_information(REPORT_DTC_SNAPSHOT_RECORD_BY_DTC_NUMBER, NRC_REQUEST_OUT_OF_RANGE);
        return;
    }

    // Send a positive response with the DTC and the corresponding snapshot
    send_positive_response_read_dtc_information(REPORT_DTC_SNAPSHOT_RECORD_BY_DTC_NUMBER, &dtc_record, 1);
}



void report_dtc_stored_data_by_record_number(uint8_t* data, uint8_t data_length) {
    // Check the length of the received data
    if (data_length != 2) {
        send_negative_response_read_dtc_information(REPORT_DTC_STORED_DATA_BY_RECORD_NUMBER, NRC_INCORRECT_MESSAGE_LENGTH);
        return;
    }

    uint16_t dtc_stored_data_record_number = (data[0] << 8) | data[1];

    uint8_t found = 0;
    DTC_Record dtc_record;  // No dynamic allocation here

    // Browse the list of DTCs to find the record corresponding to the provided number
    for (uint8_t i = 0; i < MAX_DTC_COUNT; i++) {
        if (stored_dtc_list[i].storedDataRecordNumber == dtc_stored_data_record_number || dtc_stored_data_record_number == 0xFF) {
            dtc_record.dtcNumber = stored_dtc_list[i].dtcNumber;
            dtc_record.status = stored_dtc_list[i].status;
            dtc_record.storedDataRecordNumber = stored_dtc_list[i].storedDataRecordNumber;

            // Copy the data into the fixed array
            for (uint8_t j = 0; j < stored_dtc_list[i].storedDataLength; j++) {
                dtc_record.snapshotData[j] = stored_dtc_list[i].storedData[j];
            }
            dtc_record.snapshotDataLength = stored_dtc_list[i].storedDataLength;
            found = 1;
            break;
        }
    }

    // If no record is found
    if (!found) {
        send_negative_response_read_dtc_information(REPORT_DTC_STORED_DATA_BY_RECORD_NUMBER, NRC_REQUEST_OUT_OF_RANGE);
        return;
    }

    // Send a positive response with the data found
    send_positive_response_read_dtc_information(REPORT_DTC_STORED_DATA_BY_RECORD_NUMBER, &dtc_record, 1);
}



void report_dtc_ext_data_record_by_dtc_number(uint8_t* data, uint8_t data_length) {
    // Check the length of the received data (DTCMaskRecord + ExtDataRecordNumber = 4 bytes expected)
    if (data_length != 4) {
        send_negative_response_read_dtc_information(REPORT_DTC_EXT_DATA_RECORD_BY_DTC_NUMBER, NRC_INCORRECT_MESSAGE_LENGTH);
        return;
    }

    // Extract the DTC number from the client request (3 bytes)
    uint32_t dtc_mask_record = (data[0] << 16) | (data[1] << 8) | data[2];
    uint8_t ext_data_record_number = data[3];  // Extended data record number

    uint8_t found = 0;
    DTC_Record dtc_record;

    // Browse the list of DTCs to find the DTC matching the mask and the extended data record number
    for (uint8_t i = 0; i < MAX_DTC_COUNT; i++) {
        if (stored_dtc_list[i].dtcNumber == dtc_mask_record) {
            // If an extended data record is found
            if (stored_dtc_list[i].storedDataRecordNumber == ext_data_record_number || ext_data_record_number == 0xFF) {
                dtc_record.dtcNumber = stored_dtc_list[i].dtcNumber;
                dtc_record.status = stored_dtc_list[i].status;
                dtc_record.storedDataRecordNumber = stored_dtc_list[i].storedDataRecordNumber;

                // Copy the extended data directly into the `snapshotData` array
                dtc_record.snapshotDataLength = stored_dtc_list[i].storedDataLength;
                if (dtc_record.snapshotDataLength > MAX_DTC_EXT_DATA_RECORD_SIZE) {
                    send_negative_response_read_dtc_information(REPORT_DTC_EXT_DATA_RECORD_BY_DTC_NUMBER, NRC_REQUEST_OUT_OF_RANGE);
                    return;
                }
                for (uint8_t j = 0; j < dtc_record.snapshotDataLength; j++) {
                    dtc_record.snapshotData[j] = stored_dtc_list[i].storedData[j];
                }

                found = 1;
                break;
            }
        }
    }

    // If no record is found, send an NRC
    if (!found) {
        send_negative_response_read_dtc_information(REPORT_DTC_EXT_DATA_RECORD_BY_DTC_NUMBER, NRC_REQUEST_OUT_OF_RANGE);
        return;
    }

    // Send a positive response with the DTC and the corresponding extended data
    send_positive_response_read_dtc_information(REPORT_DTC_EXT_DATA_RECORD_BY_DTC_NUMBER, &dtc_record, 1);
}



void report_number_of_dtc_by_severity_mask(uint8_t* data, uint8_t data_length) {
    // Check that the length of the data is correct (status + severity = 2 bytes expected)
    if (data_length != 2) {
        send_negative_response_read_dtc_information(REPORT_NUMBER_OF_DTC_BY_SEVERITY_MASK, NRC_INCORRECT_MESSAGE_LENGTH);
        return;
    }

    uint8_t dtc_status_mask = data[0];
    uint8_t dtc_severity_mask = data[1];
    uint16_t dtc_count = 0;

    // Browse all DTCs and count those that correspond to the status and severity mask
    for (uint8_t i = 0; i < MAX_DTC_COUNT; i++) {
        if (((stored_dtc_list[i].status & dtc_status_mask) != 0) &&
            ((stored_dtc_list[i].severity & dtc_severity_mask) != 0)) {
            dtc_count++;
        }
    }

    // Create the response
    uint8_t response[3];
    response[0] = get_dtc_status_availability_mask();
    response[1] = (dtc_count >> 8) & 0xFF;  // Octet high of the DTC count
    response[2] = dtc_count & 0xFF;         // Octet low of the DTC count

    // Send the response to the client
    send_positive_response_read_dtc_information(REPORT_NUMBER_OF_DTC_BY_SEVERITY_MASK, (DTC_Record*)response, 0);
}




void report_dtc_by_severity_mask_record(uint8_t* data, uint8_t data_length) {
    // Check that the length of the data is correct (status + severity = 2 bytes expected)
    if (data_length != 2) {
        send_negative_response_read_dtc_information(REPORT_DTC_BY_SEVERITY_MASK_RECORD, NRC_INCORRECT_MESSAGE_LENGTH);
        return;
    }

    uint8_t dtc_status_mask = data[0];
    uint8_t dtc_severity_mask = data[1];
    uint8_t found_dtc_count = 0;

    // Dynamically allocate memory for the DTC records
    DTC_Record* dtc_records = (DTC_Record*)malloc(MAX_DTC_COUNT * sizeof(DTC_Record));
    if (dtc_records == NULL) {
        Error_Handler();
        return;
    }

    // Browse all DTCs and add those that correspond to the status and severity mask
    for (uint8_t i = 0; i < MAX_DTC_COUNT; i++) {
        if (((stored_dtc_list[i].status & dtc_status_mask) != 0) &&
            ((stored_dtc_list[i].severity & dtc_severity_mask) != 0)) {
            dtc_records[found_dtc_count] = stored_dtc_list[i];
            found_dtc_count++;
        }
    }

    // If no DTC is found, send an NRC
    if (found_dtc_count == 0) {
        free(dtc_records);
        send_negative_response_read_dtc_information(REPORT_DTC_BY_SEVERITY_MASK_RECORD, NRC_CONDITIONS_NOT_CORRECT);
        return;
    }

    // Send the response with the corresponding DTC records
    send_positive_response_read_dtc_information(REPORT_DTC_BY_SEVERITY_MASK_RECORD, dtc_records, found_dtc_count);

    // Free the dynamically allocated memory
    free(dtc_records);
}



void report_severity_information_of_dtc(uint8_t* data, uint8_t data_length) {
    // Check that the length of the data is correct (3 bytes for the DTC number)
    if (data_length != 3) {
        send_negative_response_read_dtc_information(REPORT_SEVERITY_INFORMATION_OF_DTC, NRC_INCORRECT_MESSAGE_LENGTH);
        return;
    }

    // Extract the DTC number from the client request (3 bytes)
    uint32_t dtc_mask_record = (data[0] << 16) | (data[1] << 8) | data[2];
    uint8_t found = 0;
    DTC_Record dtc_record;

    // Browse all DTCs to find the one corresponding to the DTCMaskRecord
    for (uint8_t i = 0; i < MAX_DTC_COUNT; i++) {
        if (stored_dtc_list[i].dtcNumber == dtc_mask_record) {
            dtc_record = stored_dtc_list[i];
            found = 1;
            break;
        }
    }

    // If no DTC corresponding is found, send an NRC
    if (!found) {
        send_negative_response_read_dtc_information(REPORT_SEVERITY_INFORMATION_OF_DTC, NRC_REQUEST_OUT_OF_RANGE);
        return;
    }

    // Send a positive response with the severity information for the found DTC
    send_positive_response_read_dtc_information(REPORT_SEVERITY_INFORMATION_OF_DTC, &dtc_record, 1);
}



void report_supported_dtc(uint8_t* data, uint8_t data_length) {
    // No additional data is expected
    if (data_length != 0) {
        send_negative_response_read_dtc_information(REPORT_SUPPORTED_DTC, NRC_INCORRECT_MESSAGE_LENGTH);
        return;
    }

    uint8_t dtc_count = 0;

    // Dynamically allocate memory for storing the DTCs
    DTC_Record* dtc_records = (DTC_Record*)malloc(MAX_DTC_COUNT * sizeof(DTC_Record));
    if (dtc_records == NULL) {
        Error_Handler();
        return;
    }

    // Browse the DTCs to recover all those supported
    for (uint8_t i = 0; i < MAX_DTC_COUNT; i++) {
        if (stored_dtc_list[i].status != 0) {
            dtc_records[dtc_count] = stored_dtc_list[i];
            dtc_count++;
        }
    }

    // If no DTC is found, send an NRC
    if (dtc_count == 0) {
        free(dtc_records);
        send_negative_response_read_dtc_information(REPORT_SUPPORTED_DTC, NRC_CONDITIONS_NOT_CORRECT);
        return;
    }

    // Send the supported DTCs
    send_positive_response_read_dtc_information(REPORT_SUPPORTED_DTC, dtc_records, dtc_count);

    // Free the memory
    free(dtc_records);
}


void report_first_test_failed_dtc(uint8_t* data, uint8_t data_length) {
    if (data_length != 0) {
        send_negative_response_read_dtc_information(REPORT_FIRST_TEST_FAILED_DTC, NRC_INCORRECT_MESSAGE_LENGTH);
        return;
    }

    uint8_t found = 0;
    DTC_Record dtc_record;

    // Browse the DTCs to find the first one that failed a test
    for (uint8_t i = 0; i < MAX_DTC_COUNT; i++) {
        if (stored_dtc_list[i].status & DTC_TEST_FAILED) {
            dtc_record = stored_dtc_list[i];
            found = 1;
            break;
        }
    }

    // If no DTC is found, send an NRC
    if (!found) {
        send_negative_response_read_dtc_information(REPORT_FIRST_TEST_FAILED_DTC, NRC_CONDITIONS_NOT_CORRECT);
        return;
    }

    // Send the response with the first DTC that failed
    send_positive_response_read_dtc_information(REPORT_FIRST_TEST_FAILED_DTC, &dtc_record, 1);
}



void report_first_confirmed_dtc(uint8_t* data, uint8_t data_length) {
    if (data_length != 0) {
        send_negative_response_read_dtc_information(REPORT_FIRST_CONFIRMED_DTC, NRC_INCORRECT_MESSAGE_LENGTH);
        return;
    }

    uint8_t found = 0;
    DTC_Record dtc_record;

    // Browse the DTCs to find the first confirmed
    for (uint8_t i = 0; i < MAX_DTC_COUNT; i++) {
        if (stored_dtc_list[i].status & DTC_CONFIRMED) {
            dtc_record = stored_dtc_list[i];
            found = 1;
            break;
        }
    }

    // If no DTC confirmed is found, send an NRC
    if (!found) {
        send_negative_response_read_dtc_information(REPORT_FIRST_CONFIRMED_DTC, NRC_CONDITIONS_NOT_CORRECT);
        return;
    }

    // Send the response with the first DTC confirmed
    send_positive_response_read_dtc_information(REPORT_FIRST_CONFIRMED_DTC, &dtc_record, 1);
}



void report_most_recent_test_failed_dtc(uint8_t* data, uint8_t data_length) {
    if (data_length != 0) {
        send_negative_response_read_dtc_information(REPORT_MOST_RECENT_TEST_FAILED_DTC, NRC_INCORRECT_MESSAGE_LENGTH);
        return;
    }

    uint8_t found = 0;
    DTC_Record* dtc_record = (DTC_Record*)malloc(sizeof(DTC_Record));
    if (dtc_record == NULL) {
        Error_Handler();
        return;
    }

    // Browse the DTCs to find the most recent that failed
    for (int8_t i = MAX_DTC_COUNT - 1; i >= 0; i--) {
        if (stored_dtc_list[i].status & DTC_TEST_FAILED) {
            *dtc_record = stored_dtc_list[i];
            found = 1;
            break;
        }
    }

    // If no DTC failed is found, send an NRC
    if (!found) {
        free(dtc_record);
        send_negative_response_read_dtc_information(REPORT_MOST_RECENT_TEST_FAILED_DTC, NRC_CONDITIONS_NOT_CORRECT);
        return;
    }

    // Send the response with the most recent DTC that failed
    send_positive_response_read_dtc_information(REPORT_MOST_RECENT_TEST_FAILED_DTC, dtc_record, 1);
    free(dtc_record);
}



void report_most_recent_confirmed_dtc(uint8_t* data, uint8_t data_length) {
    if (data_length != 0) {
        send_negative_response_read_dtc_information(REPORT_MOST_RECENT_CONFIRMED_DTC, NRC_INCORRECT_MESSAGE_LENGTH);
        return;
    }

    uint8_t found = 0;
    DTC_Record* dtc_record = (DTC_Record*)malloc(sizeof(DTC_Record));
    if (dtc_record == NULL) {
        Error_Handler();
        return;
    }

    // Browse the DTCs to find the most recent confirmed
    for (int8_t i = MAX_DTC_COUNT - 1; i >= 0; i--) {
        if (stored_dtc_list[i].status & DTC_CONFIRMED) {
            *dtc_record = stored_dtc_list[i];
            found = 1;
            break;
        }
    }

    // If no DTC confirmed is found, send an NRC
    if (!found) {
        free(dtc_record);
        send_negative_response_read_dtc_information(REPORT_MOST_RECENT_CONFIRMED_DTC, NRC_CONDITIONS_NOT_CORRECT);
        return;
    }

    // Send the response with the most recent DTC confirmed
    send_positive_response_read_dtc_information(REPORT_MOST_RECENT_CONFIRMED_DTC, dtc_record, 1);
    free(dtc_record);
}




void report_mirror_memory_dtc_by_status_mask(uint8_t* data, uint8_t data_length) {
    if (data_length != 1) { // Check if the status mask is provided
        send_negative_response_read_dtc_information(REPORT_MIRROR_MEMORY_DTC_BY_STATUS_MASK, NRC_INCORRECT_MESSAGE_LENGTH);
        return;
    }

    uint8_t status_mask = data[0]; // The status mask sent by the client
    uint8_t dtc_count = 0;

    // Dynamically allocate memory for the DTCs
    DTC_Record* dtc_records = (DTC_Record*)malloc(MAX_DTC_COUNT * sizeof(DTC_Record));
    if (dtc_records == NULL) {
        Error_Handler();
        return;
    }

    // Browse the DTCs in the mirror memory and filter by the mask
    for (uint8_t i = 0; i < MAX_DTC_COUNT; i++) {
        if ((mirror_dtc_list[i].status & status_mask) != 0) {
            dtc_records[dtc_count] = mirror_dtc_list[i];
            dtc_count++;
        }
    }

    if (dtc_count == 0) {
        free(dtc_records);
        send_negative_response_read_dtc_information(REPORT_MIRROR_MEMORY_DTC_BY_STATUS_MASK, NRC_CONDITIONS_NOT_CORRECT);
        return;
    }

    // Send the response with the DTCs in the mirror memory
    send_positive_response_read_dtc_information(REPORT_MIRROR_MEMORY_DTC_BY_STATUS_MASK, dtc_records, dtc_count);
    free(dtc_records);
}



void report_mirror_memory_dtc_ext_data_record(uint8_t* data, uint8_t data_length) {
    if (data_length != 4) {
        send_negative_response_read_dtc_information(REPORT_MIRROR_MEMORY_DTC_EXT_DATA_RECORD, NRC_INCORRECT_MESSAGE_LENGTH);
        return;
    }

    // Extract the DTC number and the record number of the extended data
    uint32_t dtc_mask_record = (data[0] << 16) | (data[1] << 8) | data[2];
    uint8_t ext_data_record_number = data[3];

    uint8_t found = 0;
    DTC_Record* dtc_record = NULL;

    // Browse the DTCs in the mirror memory
    for (uint8_t i = 0; i < MAX_DTC_COUNT; i++) {
        if (mirror_dtc_list[i].dtcNumber == dtc_mask_record && mirror_dtc_list[i].storedDataRecordNumber == ext_data_record_number) {
            dtc_record = &mirror_dtc_list[i];
            found = 1;
            break;
        }
    }

    // If no DTC is found
    if (!found) {
        send_negative_response_read_dtc_information(REPORT_MIRROR_MEMORY_DTC_EXT_DATA_RECORD, NRC_REQUEST_OUT_OF_RANGE);
        return;
    }

    // Send a positive response with the DTC and the extended data
    send_positive_response_read_dtc_information(REPORT_MIRROR_MEMORY_DTC_EXT_DATA_RECORD, dtc_record, 1);
}




void report_number_of_mirror_memory_dtc_by_status_mask(uint8_t* data, uint8_t data_length) {
    if (data_length != 1) {
        send_negative_response_read_dtc_information(REPORT_NUMBER_OF_MIRROR_MEMORY_DTC_BY_STATUS_MASK, NRC_INCORRECT_MESSAGE_LENGTH);
        return;
    }

    uint8_t status_mask = data[0];
    uint16_t dtc_count = 0;

    // Browse the DTCs to count those corresponding to the status mask
    for (uint8_t i = 0; i < MAX_DTC_COUNT; i++) {
        if ((mirror_dtc_list[i].status & status_mask) != 0) {
            dtc_count++;
        }
    }

    if (dtc_count == 0) {
        send_negative_response_read_dtc_information(REPORT_NUMBER_OF_MIRROR_MEMORY_DTC_BY_STATUS_MASK, NRC_CONDITIONS_NOT_CORRECT);
        return;
    }

    // Prepare the response
    uint8_t response[4];
    response[0] = get_dtc_status_availability_mask();
    response[1] = get_dtc_format_identifier();
    response[2] = (dtc_count >> 8) & 0xFF;
    response[3] = dtc_count & 0xFF;

    send_can_message(response, 4);
    // send_uart_message(response, 4);
}



void report_number_of_emissions_obd_dtc_by_status_mask(uint8_t* data, uint8_t data_length) {
    if (data_length != 1) {  // The status mask must be of length 1 byte
        send_negative_response_read_dtc_information(REPORT_NUMBER_OF_EMISSIONS_OBD_DTC_BY_STATUS_MASK, NRC_INCORRECT_MESSAGE_LENGTH);
        return;
    }

    uint8_t status_mask = data[0];
    uint16_t dtc_count = 0;

    // Browse the list of DTCs
    for (uint8_t i = 0; i < MAX_DTC_COUNT; i++) {
        if (stored_dtc_list[i].isEmissionRelated && (stored_dtc_list[i].status & status_mask) != 0) {
            dtc_count++;
        }
    }

    // Create a DTC record to send the response
    DTC_Record dtc_record;
    dtc_record.dtcNumber = 0;  // or another number according to the structure definition
    dtc_record.status = get_dtc_status_availability_mask();
    dtc_record.snapshotDataLength = 4;
    dtc_record.snapshotData[0] = get_dtc_status_availability_mask();
    dtc_record.snapshotData[1] = get_dtc_format_identifier();
    dtc_record.snapshotData[2] = (dtc_count >> 8) & 0xFF;  // Octet high of the DTC count
    dtc_record.snapshotData[3] = dtc_count & 0xFF;  // Octet low of the DTC count

    // Send the response positive
    send_positive_response_read_dtc_information(REPORT_NUMBER_OF_EMISSIONS_OBD_DTC_BY_STATUS_MASK, &dtc_record, 1);
}




void report_emissions_obd_dtc_by_status_mask(uint8_t* data, uint8_t data_length) {
    if (data_length != 1) {
        send_negative_response_read_dtc_information(REPORT_EMISSIONS_OBD_DTC_BY_STATUS_MASK, NRC_INCORRECT_MESSAGE_LENGTH);
        return;
    }

    uint8_t status_mask = data[0];
    DTC_Record* dtc_records = (DTC_Record*)malloc(MAX_DTC_COUNT * sizeof(DTC_Record));
    if (dtc_records == NULL) {
        Error_Handler();
        return;
    }

    uint8_t record_count = 0;
    for (uint8_t i = 0; i < MAX_DTC_COUNT; i++) {
        if (stored_dtc_list[i].isEmissionRelated && (stored_dtc_list[i].status & status_mask) != 0) {
            dtc_records[record_count++] = stored_dtc_list[i];
        }
    }

    if (record_count == 0) {
        send_negative_response_read_dtc_information(REPORT_EMISSIONS_OBD_DTC_BY_STATUS_MASK, NRC_CONDITIONS_NOT_CORRECT);
    } else {
        send_positive_response_read_dtc_information(REPORT_EMISSIONS_OBD_DTC_BY_STATUS_MASK, dtc_records, record_count);
    }

    free(dtc_records);
}



void report_dtc_fault_detection_counter(uint8_t* data, uint8_t data_length) {
    if (data_length != 0) {
        send_negative_response_read_dtc_information(REPORT_DTC_FAULT_DETECTION_COUNTER, NRC_INCORRECT_MESSAGE_LENGTH);
        return;
    }

    DTC_Record* dtc_records = (DTC_Record*)malloc(MAX_DTC_COUNT * sizeof(DTC_Record));
    if (dtc_records == NULL) {
        Error_Handler();
        return;
    }

    uint8_t record_count = 0;
    for (uint8_t i = 0; i < MAX_DTC_COUNT; i++) {
        if (stored_dtc_list[i].faultDetectionCounter > 0) {
            dtc_records[record_count++] = stored_dtc_list[i];
        }
    }

    if (record_count == 0) {
        send_negative_response_read_dtc_information(REPORT_DTC_FAULT_DETECTION_COUNTER, NRC_CONDITIONS_NOT_CORRECT);
    } else {
        send_positive_response_read_dtc_information(REPORT_DTC_FAULT_DETECTION_COUNTER, dtc_records, record_count);
    }

    free(dtc_records);
}



void report_dtc_with_permanent_status(uint8_t* data, uint8_t data_length) {
    if (data_length != 0) {
        send_negative_response_read_dtc_information(REPORT_DTC_WITH_PERMANENT_STATUS, NRC_INCORRECT_MESSAGE_LENGTH);
        return;
    }

    DTC_Record* dtc_records = (DTC_Record*)malloc(MAX_DTC_COUNT * sizeof(DTC_Record));
    if (dtc_records == NULL) {
        Error_Handler();
        return;
    }

    uint8_t record_count = 0;
    for (uint8_t i = 0; i < MAX_DTC_COUNT; i++) {
        if (stored_dtc_list[i].status & DTC_STATUS_PERMANENT) {
            dtc_records[record_count++] = stored_dtc_list[i];
        }
    }

    if (record_count == 0) {
        send_negative_response_read_dtc_information(REPORT_DTC_WITH_PERMANENT_STATUS, NRC_CONDITIONS_NOT_CORRECT);
    } else {
        send_positive_response_read_dtc_information(REPORT_DTC_WITH_PERMANENT_STATUS, dtc_records, record_count);
    }

    free(dtc_records);
}


void report_dtc_ext_data_record_by_record_number(uint8_t* data, uint8_t data_length) {
    if (data_length != 2) {  // The record number of the extended data must have 2 bytes
        send_negative_response_read_dtc_information(REPORT_DTC_EXT_DATA_RECORD_BY_RECORD_NUMBER, NRC_INCORRECT_MESSAGE_LENGTH);
        return;
    }

    uint16_t record_number = (data[0] << 8) | data[1];
    uint8_t found = 0;
    DTC_Record* dtc_record = NULL;

    for (uint8_t i = 0; i < MAX_DTC_COUNT; i++) {
        if (stored_dtc_list[i].storedDataRecordNumber == record_number) {
            dtc_record = &stored_dtc_list[i];
            found = 1;
            break;
        }
    }

    if (!found) {
        send_negative_response_read_dtc_information(REPORT_DTC_EXT_DATA_RECORD_BY_RECORD_NUMBER, NRC_REQUEST_OUT_OF_RANGE);
    } else {
        send_positive_response_read_dtc_information(REPORT_DTC_EXT_DATA_RECORD_BY_RECORD_NUMBER, dtc_record, 1);
    }
}


void report_user_def_memory_dtc_by_status_mask(uint8_t* data, uint8_t data_length) {
    if (data_length != 1) {
        send_negative_response_read_dtc_information(REPORT_USER_DEF_MEMORY_DTC_BY_STATUS_MASK, NRC_INCORRECT_MESSAGE_LENGTH);
        return;
    }

    uint8_t status_mask = data[0];
    DTC_Record* dtc_records = (DTC_Record*)malloc(MAX_DTC_COUNT * sizeof(DTC_Record));
    if (dtc_records == NULL) {
        Error_Handler();
        return;
    }

    uint8_t record_count = 0;
    for (uint8_t i = 0; i < MAX_DTC_COUNT; i++) {
        if (user_defined_memory_list[i].status & status_mask) {
            dtc_records[record_count++] = user_defined_memory_list[i];
        }
    }

    if (record_count == 0) {
        send_negative_response_read_dtc_information(REPORT_USER_DEF_MEMORY_DTC_BY_STATUS_MASK, NRC_CONDITIONS_NOT_CORRECT);
    } else {
        send_positive_response_read_dtc_information(REPORT_USER_DEF_MEMORY_DTC_BY_STATUS_MASK, dtc_records, record_count);
    }

    free(dtc_records);
}




void report_user_def_memory_dtc_snapshot_record(uint8_t* data, uint8_t data_length) {
    // Check the length of the received data (DTCMaskRecord + SnapshotRecordNumber)
    if (data_length != 4) {
        send_negative_response_read_dtc_information(REPORT_USER_DEF_MEMORY_DTC_SNAPSHOT_RECORD, NRC_INCORRECT_MESSAGE_LENGTH);
        return;
    }

    // Extract the DTCMaskRecord and the snapshot number
    uint32_t dtc_mask_record = (data[0] << 16) | (data[1] << 8) | data[2];
    uint8_t snapshot_record_number = data[3];

    uint8_t found = 0;
    DTC_Record dtc_record;

    // Browse the user memory to find the DTC
    for (uint8_t i = 0; i < MAX_DTC_COUNT; i++) {
        if (user_defined_memory_list[i].dtcNumber == dtc_mask_record && user_defined_memory_list[i].snapshotRecordNumber == snapshot_record_number) {
            dtc_record = user_defined_memory_list[i];
            found = 1;
            break;
        }
    }

    // If no record is found
    if (!found) {
        send_negative_response_read_dtc_information(REPORT_USER_DEF_MEMORY_DTC_SNAPSHOT_RECORD, NRC_REQUEST_OUT_OF_RANGE);
        return;
    }

    // Send the response positive with the snapshot
    send_positive_response_read_dtc_information(REPORT_USER_DEF_MEMORY_DTC_SNAPSHOT_RECORD, &dtc_record, 1);
}




void report_user_def_memory_dtc_ext_data_record(uint8_t* data, uint8_t data_length) {
    // Check the length of the received data
    if (data_length != 4) {
        send_negative_response_read_dtc_information(REPORT_USER_DEF_MEMORY_DTC_EXT_DATA_RECORD, NRC_INCORRECT_MESSAGE_LENGTH);
        return;
    }

    // Extract the DTCMaskRecord and the record number of the extended data
    uint32_t dtc_mask_record = (data[0] << 16) | (data[1] << 8) | data[2];
    uint8_t ext_data_record_number = data[3];

    uint8_t found = 0;
    DTC_Record dtc_record;

    // Browse the user memory to find the DTC
    for (uint8_t i = 0; i < MAX_DTC_COUNT; i++) {
        if (user_defined_memory_list[i].dtcNumber == dtc_mask_record && user_defined_memory_list[i].storedDataRecordNumber == ext_data_record_number) {
            dtc_record = user_defined_memory_list[i];
            found = 1;
            break;
        }
    }

    // If no record is found
    if (!found) {
        send_negative_response_read_dtc_information(REPORT_USER_DEF_MEMORY_DTC_EXT_DATA_RECORD, NRC_REQUEST_OUT_OF_RANGE);
        return;
    }

    // Send the response positive with the extended data
    send_positive_response_read_dtc_information(REPORT_USER_DEF_MEMORY_DTC_EXT_DATA_RECORD, &dtc_record, 1);
}




void report_wwh_obd_dtc_by_mask_record(uint8_t* data, uint8_t data_length) {
    if (data_length != 1) {  // The status mask must be of length 1 byte
        send_negative_response_read_dtc_information(REPORT_WWH_OBD_DTC_BY_MASK_RECORD, NRC_INCORRECT_MESSAGE_LENGTH);
        return;
    }

    uint8_t status_mask = data[0];
    uint8_t found = 0;
    DTC_Record* dtc_records = (DTC_Record*)malloc(MAX_DTC_COUNT * sizeof(DTC_Record));

    if (dtc_records == NULL) {
        Error_Handler();
        return;
    }

    uint8_t record_count = 0;

    // Browse the memory to find the DTCs WWH-OBD
    for (uint8_t i = 0; i < MAX_DTC_COUNT; i++) {
        if (stored_dtc_list[i].isEmissionRelated && (stored_dtc_list[i].status & status_mask) != 0) {
            dtc_records[record_count++] = stored_dtc_list[i];
            found = 1;
        }
    }

    // If no DTC is found
    if (!found) {
        send_negative_response_read_dtc_information(REPORT_WWH_OBD_DTC_BY_MASK_RECORD, NRC_CONDITIONS_NOT_CORRECT);
    } else {
        send_positive_response_read_dtc_information(REPORT_WWH_OBD_DTC_BY_MASK_RECORD, dtc_records, record_count);
    }

    free(dtc_records);
}



void report_wwh_obd_dtc_with_permanent_status(uint8_t* data, uint8_t data_length) {
    if (data_length != 0) {
        send_negative_response_read_dtc_information(REPORT_WWH_OBD_DTC_WITH_PERMANENT_STATUS, NRC_INCORRECT_MESSAGE_LENGTH);
        return;
    }

    uint8_t found = 0;
    DTC_Record* dtc_records = (DTC_Record*)malloc(MAX_DTC_COUNT * sizeof(DTC_Record));

    if (dtc_records == NULL) {
        Error_Handler();
        return;
    }

    uint8_t record_count = 0;

    // Browse the memory to find the DTCs WWH-OBD with permanent status
    for (uint8_t i = 0; i < MAX_DTC_COUNT; i++) {
        if (stored_dtc_list[i].status & DTC_STATUS_PERMANENT) {
            dtc_records[record_count++] = stored_dtc_list[i];
            found = 1;
        }
    }

    // If no DTC is found
    if (!found) {
        send_negative_response_read_dtc_information(REPORT_WWH_OBD_DTC_WITH_PERMANENT_STATUS, NRC_CONDITIONS_NOT_CORRECT);
    } else {
        send_positive_response_read_dtc_information(REPORT_WWH_OBD_DTC_WITH_PERMANENT_STATUS, dtc_records, record_count);
    }

    free(dtc_records);
}



uint8_t get_dtc_status_availability_mask() {
    // Return a DTC status availability mask according to your needs
    return 0xFF; // Example: All statuses are available
}
uint8_t get_dtc_format_identifier() {
    // Return a DTC format identifier (e.g., SAE J2012, ISO, etc.)
    return 0x01; // Example
}



