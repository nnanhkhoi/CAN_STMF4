#include "Data_Transmission_functional_unit.h"
#include "uds_services.c"
#include "main.h"



/****************************************************ReadDataByIdentifier*******************************************************/
uint8_t data_record_1[] = {0x12, 0x34};
uint8_t data_record_2[] = {0x56, 0x78};
uint8_t data_record_3[] = {0x9A, 0xBC};

/**
 * @brief Implements the ReadDataByIdentifier service (0x22)
 * @param data : Pointer to the request data (containing the dataIdentifiers)
 * @param data_length : Length of the data
 */
void uds_read_data_by_identifier(uint8_t* data, uint8_t data_length) {
    // Verification of minimum length and modulo 2 of the request
    if (data_length < 2 || data_length % 2 != 0) {
        send_negative_response_read_data_by_identifier(NRC_INCORRECT_MESSAGE_LENGTH);
        return;
    }

    // Verification of maximum length of the request
    if (data_length > MAX_DATA_SIZE) {
        send_negative_response_read_data_by_identifier(NRC_INCORRECT_MESSAGE_LENGTH);
        return;
    }

    uint8_t response[MAX_DATA_SIZE] = {0};  // Maximum response of 64 bytes
    uint8_t response_index = 0;  // Response fill index
    bool did_supported = false;  // Indicator to verify if at least one DID is supported

    response[response_index++] = UDS_READ_DATA_BY_IDENTIFIER + 0x40;  // Response SID 0x62

    // Loop for each dataIdentifier
    for (uint8_t i = 0; i < data_length; i += 2) {
        uint16_t did = (data[i] << 8) | data[i + 1];  // Retrieve the DID

        // Verification that the service is supported for each DID in the active session
        if (!is_service_allowed(UDS_READ_DATA_BY_IDENTIFIER)) {
            send_negative_response_read_data_by_identifier(NRC_CONDITIONS_NOT_CORRECT);
            return;
        }

        // Verification of security conditions for the DID
        if (is_security_required_for_did(did) && !uds_session.security_access_granted) {
            send_negative_response_read_data_by_identifier(NRC_SECURITY_ACCESS_DENIED);
            return;
        }

        switch (did) {
            case SUPPORTED_DID_1:
                // Add the DID and associated data to the response
                response[response_index++] = data[i];       // DID MSB
                response[response_index++] = data[i + 1];   // DID LSB
                memcpy(&response[response_index], data_record_1, sizeof(data_record_1));
                response_index += sizeof(data_record_1);
                did_supported = true;
                break;

            case SUPPORTED_DID_2:
                response[response_index++] = data[i];       // DID MSB
                response[response_index++] = data[i + 1];   // DID LSB
                memcpy(&response[response_index], data_record_2, sizeof(data_record_2));
                response_index += sizeof(data_record_2);
                did_supported = true;
                break;

            case SUPPORTED_DID_3:
                response[response_index++] = data[i];       // DID MSB
                response[response_index++] = data[i + 1];   // DID LSB
                memcpy(&response[response_index], data_record_3, sizeof(data_record_3));
                response_index += sizeof(data_record_3);
                did_supported = true;
                break;

            default:
                // If the DID is not supported, continue the loop without responding immediately
                continue;
        }

        // Check if the response size exceeds the transport protocol limit
        if (response_index > MAX_DATA_SIZE) {
            send_negative_response_read_data_by_identifier(NRC_RESPONSE_TOO_LONG);
            return;
        }
    }

    // If no DID is supported, return a negative response
    if (!did_supported) {
        send_negative_response_read_data_by_identifier(NRC_REQUEST_OUT_OF_RANGE);
        return;
    }

    // Send the positive response with all processed DIDs
    send_can_message(response, response_index);
    // send_uart_message(response, response_index);
}

bool is_security_required_for_did(uint16_t did) {
    // Add here the logic to check if a DID requires security access
    return false;  // By default, assume that DIDs do not require security
}

/**
 * @brief Send a positive response for the ReadDataByIdentifier service (0x22)
 * @param dataIdentifiers : Array of DIDs (data identifiers)
 * @param dataRecords : Array of data records associated with DIDs
 * @param number_of_dids : Number of DIDs in the response
 */
void send_positive_response_read_data_by_identifier(uint8_t* dataIdentifiers, uint8_t* dataRecords, uint8_t number_of_dids) {
    uint8_t response[MAX_DATA_SIZE] = {0};
    uint8_t index = 0;

    response[index++] = UDS_READ_DATA_BY_IDENTIFIER + 0x40;  // Response SID 0x62

    // Add the DIDs and their associated records
    for (uint8_t i = 0; i < number_of_dids; i++) {
        response[index++] = dataIdentifiers[2 * i];     // DID MSB
        response[index++] = dataIdentifiers[2 * i + 1]; // DID LSB
        response[index++] = dataRecords[2 * i];         // Record value
        response[index++] = dataRecords[2 * i + 1];     // Record value
    }

    // Send the response via CAN
    send_can_message(response, index);
    // send_uart_message(response, index);
}

/**
 * @brief Send a negative response for the ReadDataByIdentifier service (0x22)
 * @param nrc : Negative response code (NRC)
 */
void send_negative_response_read_data_by_identifier(uint8_t nrc) {
    uint8_t response[3] = {0};

    response[0] = UDS_NEGATIVE_RESPONSE;  // SID pour reponse negative 0x7F
    response[1] = UDS_READ_DATA_BY_IDENTIFIER;  // Service SID 0x22
    response[2] = nrc;  // Code de reponse negative (NRC)

    send_can_message(response, 3);
    //send_uart_message(response, 3);
}

/*********************************************************ReadMemoryByAddress********************************************************/
// Simulate memory for reading
uint8_t memory[1024] = {
    0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF, 0xAA, 0xBB,
    0xCC, 0xDD, 0xEE, 0xFF, 0xAA, 0xBB, 0xCC, 0xDD,
};

// Implements the ReadMemoryByAddress service (0x23)
void uds_read_memory_by_address(uint8_t* data, uint8_t data_length) {
    // Check minimum length
    if (data_length < 4) {
        send_negative_response_read_memory_by_address(NRC_INCORRECT_MESSAGE_LENGTH);
        return;
    }

    uint8_t addressAndLengthFormatIdentifier = data[0];
    uint8_t address_length = addressAndLengthFormatIdentifier & 0x0F; // Low nibble
    uint8_t size_length = (addressAndLengthFormatIdentifier >> 4) & 0x0F; // High nibble

    // Check if ALFID is valid
    if (address_length < 1 || address_length > 4 || size_length < 1 || size_length > 4) {
        send_negative_response_read_memory_by_address(NRC_REQUEST_OUT_OF_RANGE);
        return;
    }

    // Calculate the memory address from the provided bytes
    uint32_t memory_address = 0;
    for (int i = 0; i < address_length; i++) {
        memory_address = (memory_address << 8) | data[1 + i];
    }

    // Calculate the memory size from the provided bytes
    uint32_t memory_size = 0;
    for (int i = 0; i < size_length; i++) {
        memory_size = (memory_size << 8) | data[1 + address_length + i];
    }

    // Address range and size verification
    if (memory_address + memory_size > sizeof(memory)) {
        send_negative_response_read_memory_by_address(NRC_REQUEST_OUT_OF_RANGE);
        return;
    }

    // Check security conditions
    if (!uds_session.security_access_granted) {
        send_negative_response_read_memory_by_address(NRC_SECURITY_ACCESS_DENIED);
        return;
    }

    // Read the memory and send the positive response
    send_positive_response_read_memory_by_address(&memory[memory_address], memory_size);
}

// Function to send a positive response with the read data
void send_positive_response_read_memory_by_address(uint8_t* dataRecord, uint8_t data_length) {
    uint8_t response[MAX_DATA_SIZE] = {0};
    uint8_t index = 0;

    // Add SID 0x63 for the positive response
    response[index++] = UDS_READ_MEMORY_BY_ADDRESS + 0x40;

    // Add the read data
    memcpy(&response[index], dataRecord, data_length);
    index += data_length;

    // Send the response via CAN
    send_can_message(response, index);
    //send_uart_message(response, index);
}

// Function to send a negative response
void send_negative_response_read_memory_by_address(uint8_t nrc) {
    uint8_t response[3] = {0};

    response[0] = UDS_NEGATIVE_RESPONSE;  // SID for negative response 0x7F
    response[1] = UDS_READ_MEMORY_BY_ADDRESS;  // Service SID 0x23
    response[2] = nrc;  // Negative response code (NRC)

    send_can_message(response, 3);
    //send_uart_message(response, 3);

}
/****************************************************ReadDataByPeriodicIdentifier***********************************************/
// Initialization of the periodic PIDs list
PeriodicPIDInfo periodic_pid_list[MAX_PERIODIC_PIDS];

// Main function to manage the ReadDataByPeriodicIdentifier request
void uds_read_data_by_periodic_identifier(uint8_t* data, uint8_t data_length) {
    // Length verification of the minimum message
    if (data_length < 2) {
        send_negative_response_read_data_by_periodic_identifier(NRC_INCORRECT_MESSAGE_LENGTH);
        return;
    }

    uint8_t transmissionMode = data[1]; // Byte 2 : Transmission mode
    if (transmissionMode < UDS_TRANSMISSION_MODE_SLOW || transmissionMode > UDS_TRANSMISSION_MODE_STOP) {
        send_negative_response_read_data_by_periodic_identifier(NRC_REQUEST_OUT_OF_RANGE);
        return;
    }

    // Length verification for each mode
    if (transmissionMode == UDS_TRANSMISSION_MODE_STOP && data_length < 2) {
        send_negative_response_read_data_by_periodic_identifier(NRC_INCORRECT_MESSAGE_LENGTH);
        return;
    } else if (transmissionMode != UDS_TRANSMISSION_MODE_STOP && data_length < 3) {
        send_negative_response_read_data_by_periodic_identifier(NRC_INCORRECT_MESSAGE_LENGTH);
        return;
    }

    // Boucle pour traiter tous les periodicDataIdentifiers (PIDs) fournis
    for (uint8_t i = 2; i < data_length; i++) {
        uint8_t periodicDataIdentifier = data[i];

        // Verifier si le PID est valide dans la session active
        if (!is_pid_supported_in_session(periodicDataIdentifier)) {
            send_negative_response_read_data_by_periodic_identifier(NRC_REQUEST_OUT_OF_RANGE);
            return;
        }

        // Verifier la securite si necessaire
        if (!is_security_granted_for_pid(periodicDataIdentifier)) {
            send_negative_response_read_data_by_periodic_identifier(NRC_SECURITY_ACCESS_DENIED);
            return;
        }

        // Stop transmission mode handling
        if (transmissionMode == UDS_TRANSMISSION_MODE_STOP) {
            stop_periodic_transmission(periodicDataIdentifier);
        } else {
            // Start periodic transmission for this PID
            start_periodic_transmission(periodicDataIdentifier, transmissionMode);
        }
    }

    // Send the initial positive response
    send_positive_response_read_data_by_periodic_identifier();
}

// Function to start periodic transmission
void start_periodic_transmission(uint8_t pid, uint8_t mode) {
    // Iterate through the periodic PIDs list to find a free location
    for (int i = 0; i < MAX_PERIODIC_PIDS; i++) {
        if (!periodic_pid_list[i].isActive) {
            // Initialize parameters for this PID
            periodic_pid_list[i].periodicDataIdentifier = pid;
            periodic_pid_list[i].transmissionMode = mode;
            periodic_pid_list[i].isActive = true;
            return;
        }
    }

    // If no location is available, send a negative response
    send_negative_response_read_data_by_periodic_identifier(NRC_CONDITIONS_NOT_CORRECT);
}

// Function to stop periodic transmission for a given PID
void stop_periodic_transmission(uint8_t pid) {
    for (int i = 0; i < MAX_PERIODIC_PIDS; i++) {
        if (periodic_pid_list[i].periodicDataIdentifier == pid && periodic_pid_list[i].isActive) {
            periodic_pid_list[i].isActive = false;
            return;
        }
    }
}

// Fonction pour arreter toutes les transmissions periodiques
void stop_all_periodic_transmissions(void) {
    for (int i = 0; i < MAX_PERIODIC_PIDS; i++) {
        periodic_pid_list[i].isActive = false;
    }
}

// Fonction pour envoyer une reponse positive initiale
void send_positive_response_read_data_by_periodic_identifier(void) {
    uint8_t response[2];
    response[0] = UDS_READ_DATA_BY_PERIODIC_IDENTIFIER + 0x40; // Positive response SID
    response[1] = 0x00; // No data required in the positive response

    // Send the response via CAN
    send_can_message(response, 2);
    //send_uart_message(response, 2);
}

// Function to send a negative response
void send_negative_response_read_data_by_periodic_identifier(uint8_t nrc) {
    uint8_t response[3];
    response[0] = UDS_NEGATIVE_RESPONSE; // SID for negative response
    response[1] = UDS_READ_DATA_BY_PERIODIC_IDENTIFIER; // Service SID
    response[2] = nrc; // NRC code

    send_can_message(response, 3);
    //send_uart_message(response, 3);
}

// Check if a PID is supported in the active session
bool is_pid_supported_in_session(uint8_t pid) {
    // Check supported PIDs (this logic can be modified depending on the session)
    if (pid == 0xE3 || pid == 0x24 || pid == 0x01 || pid == 0x02) {
        return true;
    }
    return false;
}

// Check security authorizations for a PID
bool is_security_granted_for_pid(uint8_t pid) {
    // Simulate a security check. Modify this function as needed.
    // For example, check if the pid requires a secure session.
    return uds_session.security_access_granted;
}

/*************************************************DynamicallyDefineDataIdentifier***********************************************/
// Storage of dynamic data identifiers
static DynamicallyDefinedIdentifier dynamic_did_list[MAX_DYNAMIC_DIDS];
static uint8_t dynamic_did_count = 0;

// Fonction pour gerer le service DynamicallyDefineDataIdentifier (0x2C)
void uds_dynamically_define_data_identifier(uint8_t sub_function, uint8_t *data, uint8_t data_length) {
    // Verifier la sous-fonction demandee
    switch (sub_function) {
        case UDS_DDDI_DEFINE_BY_IDENTIFIER:
            define_by_identifier(data, data_length);
            break;

        case UDS_DDDI_DEFINE_BY_MEMORY_ADDRESS:
            define_by_memory_address(data, data_length);
            break;

        case UDS_DDDI_CLEAR_DYNAMIC_IDENTIFIER:
            // Convert the first two bytes to a dynamic DID (uint16_t)
            if (data_length < 2) {
                send_negative_response_dynamically_define_data_identifier(sub_function, NRC_INCORRECT_MESSAGE_LENGTH);
                return;
            }
            uint16_t did_to_clear = (data[0] << 8) | data[1];
            clear_dynamically_defined_data_identifier(did_to_clear);
            break;

        default:
            // Send a negative response for an unsupported sub-function
            send_negative_response_dynamically_define_data_identifier(sub_function, NRC_SUB_FUNCTION_NOT_SUPPORTED);
            break;
    }
}

// Function to define a dynamic DID by identifier (0x01)
void define_by_identifier(uint8_t *data, uint8_t data_length) {
    if (data_length < 8) {
        // Incorrect message length
        send_negative_response_dynamically_define_data_identifier(UDS_DDDI_DEFINE_BY_IDENTIFIER, NRC_INCORRECT_MESSAGE_LENGTH);
        return;
    }

    if (dynamic_did_count >= MAX_DYNAMIC_DIDS) {
        // Exceeded maximum number of dynamic identifiers
        send_negative_response_dynamically_define_data_identifier(UDS_DDDI_DEFINE_BY_IDENTIFIER, NRC_REQUEST_OUT_OF_RANGE);
        return;
    }

    // Extract data from the request
    uint16_t dynamic_did = (data[0] << 8) | data[1];   // Dynamic DID
    uint16_t source_did = (data[2] << 8) | data[3];    // Source DID
    uint8_t position_in_source = data[4];              // Position in the source DID
    uint8_t memory_size = data[5];                     // Data size

    // Check if the DID is already defined
    if (is_dynamic_identifier_supported(dynamic_did)) {
        send_negative_response_dynamically_define_data_identifier(UDS_DDDI_DEFINE_BY_IDENTIFIER, NRC_REQUEST_OUT_OF_RANGE);
        return;
    }

    // Save the new dynamic DID
    dynamic_did_list[dynamic_did_count].dynamicDataIdentifier = dynamic_did;
    dynamic_did_list[dynamic_did_count].sourceDataIdentifier = source_did;
    dynamic_did_list[dynamic_did_count].positionInSource = position_in_source;
    dynamic_did_list[dynamic_did_count].memorySize = memory_size;

    dynamic_did_count++;

    // Send a positive response
    send_positive_response_dynamically_define_data_identifier(UDS_DDDI_DEFINE_BY_IDENTIFIER, dynamic_did);
}

// Function to define a dynamic DID by memory address (0x02)
void define_by_memory_address(uint8_t *data, uint8_t data_length) {
    if (data_length < 10) {
        send_negative_response_dynamically_define_data_identifier(UDS_DDDI_DEFINE_BY_MEMORY_ADDRESS, NRC_INCORRECT_MESSAGE_LENGTH);
        return;
    }

    if (dynamic_did_count >= MAX_DYNAMIC_DIDS) {
        send_negative_response_dynamically_define_data_identifier(UDS_DDDI_DEFINE_BY_MEMORY_ADDRESS, NRC_REQUEST_OUT_OF_RANGE);
        return;
    }

    // Extract information from the request
    uint16_t dynamic_did = (data[0] << 8) | data[1];   // Dynamic DID
    uint8_t address_format = data[2];                  // Address and size format

    // Simple validation based on address_format (Example)
    if (address_format != 0x14) {
        send_negative_response_dynamically_define_data_identifier(UDS_DDDI_DEFINE_BY_MEMORY_ADDRESS, NRC_REQUEST_OUT_OF_RANGE);
        return;
    }

    uint32_t memory_address = (data[3] << 24) | (data[4] << 16) | (data[5] << 8) | data[6]; // Memory address
    uint8_t memory_size = data[7];                     // Data size

    // Save the dynamic DID
    dynamic_did_list[dynamic_did_count].dynamicDataIdentifier = dynamic_did;
    dynamic_did_list[dynamic_did_count].positionInSource = memory_address; // Store address as position
    dynamic_did_list[dynamic_did_count].memorySize = memory_size;

    dynamic_did_count++;

    // Send a positive response
    send_positive_response_dynamically_define_data_identifier(UDS_DDDI_DEFINE_BY_MEMORY_ADDRESS, dynamic_did);
}


// Function to delete a dynamic DID (0x03)
void clear_dynamically_defined_data_identifier(uint16_t dynamicDataIdentifier) {
    // Search and delete the dynamic DID
    for (uint8_t i = 0; i < dynamic_did_count; i++) {
        if (dynamic_did_list[i].dynamicDataIdentifier == dynamicDataIdentifier) {
            // Delete the entry by shifting the other elements
            dynamic_did_list[i] = dynamic_did_list[dynamic_did_count - 1]; // Replace with the last entry
            dynamic_did_count--;
            send_positive_response_dynamically_define_data_identifier(UDS_DDDI_CLEAR_DYNAMIC_IDENTIFIER, dynamicDataIdentifier);
            return;
        }
    }

    // If the DID does not exist, send a negative response
    send_negative_response_dynamically_define_data_identifier(UDS_DDDI_CLEAR_DYNAMIC_IDENTIFIER, NRC_REQUEST_OUT_OF_RANGE);
}

// Function to check if a dynamic DID is already defined
bool is_dynamic_identifier_supported(uint16_t did) {
    for (uint8_t i = 0; i < dynamic_did_count; i++) {
        if (dynamic_did_list[i].dynamicDataIdentifier == did) {
            return true;
        }
    }
    return false;
}

// Function to send a positive response
void send_positive_response_dynamically_define_data_identifier(uint8_t sub_function, uint16_t dynamic_did) {
    uint8_t response[4];
    response[0] = UDS_DYNAMICAL_DEFINE_DATA_IDENTIFIER + 0x40; // SID for positive response
    response[1] = sub_function;
    response[2] = (dynamic_did >> 8) & 0xFF; // DID MSB
    response[3] = dynamic_did & 0xFF;        // DID LSB
    send_can_message(response, 4);
    //send_uart_message(response, 4);
}

// Function to send a negative response
void send_negative_response_dynamically_define_data_identifier(uint8_t sub_function, uint8_t nrc) {
    uint8_t response[3];
    response[0] = UDS_NEGATIVE_RESPONSE; // Negative response
    response[1] = UDS_DYNAMICAL_DEFINE_DATA_IDENTIFIER; // Service SID
    response[2] = nrc; // NRC code
    send_can_message(response, 3);
    //send_uart_message(response, 3);
}
/************************************************* WriteMemoryByAddress*********************************************************/

// Fictitious storage for DIDs, replace with the actual logic of your system
uint8_t did_storage[MAX_DATA_SIZE] = {0};

// Main function to manage the WriteDataByIdentifier service (0x2E)
void uds_write_data_by_identifier(uint8_t* data, uint8_t data_length) {
    // Minimum message length check (4 bytes: SID + DID + at least 1 byte of data)
    if (data_length < 4) {
        send_negative_response_write_data_by_identifier(NRC_INCORRECT_MESSAGE_LENGTH);
        return;
    }

    // Extract the dataIdentifier (DID) from the request
    uint16_t dataIdentifier = (data[0] << 8) | data[1];

    // Check if the DID is supported and writable in the current session
    if (!is_data_identifier_supported_for_write(dataIdentifier)) {
        send_negative_response_write_data_by_identifier(NRC_REQUEST_OUT_OF_RANGE);
        return;
    }

    // Security verification (if the DID requires secure access)
    if (is_security_required_for_did(dataIdentifier) && !uds_session.security_access_granted) {
        send_negative_response_write_data_by_identifier(NRC_SECURITY_ACCESS_DENIED);
        return;
    }

    // Check write conditions (e.g. session, ECU state)
    if (!are_conditions_correct_for_did(dataIdentifier)) {
        send_negative_response_write_data_by_identifier(NRC_CONDITIONS_NOT_CORRECT);
        return;
    }

    // Extract the dataRecord from the message
    uint8_t* dataRecord = &data[2];
    uint8_t dataRecordLength = data_length - 2;

    // Perform data writing to the specified DID
    if (!write_data_to_identifier(dataIdentifier, dataRecord, dataRecordLength)) {
        send_negative_response_write_data_by_identifier(NRC_GENERAL_PROGRAMMING_FAILURE);
        return;
    }

    // Send a positive response after successful write
    send_positive_response_write_data_by_identifier(dataIdentifier);
}

// Function to check if a DID is supported for writing
bool is_data_identifier_supported_for_write(uint16_t dataIdentifier) {
    // Implement the logic here to check if the DID is writable
    switch (dataIdentifier) {
        case SUPPORTED_DID_1:
        case SUPPORTED_DID_2:
        case SUPPORTED_DID_3:
            return true;  // These DIDs support writing
        default:
            return false; // DID not supported
    }
}



// Function to write data to the specified DID
bool write_data_to_identifier(uint16_t dataIdentifier, uint8_t* dataRecord, uint8_t dataLength) {
    // Implement the logic to write the data to the memory associated with the DID
    // Simple example: write to a fictitious memory area
    switch (dataIdentifier) {
        case SUPPORTED_DID_1:
            memcpy(&did_storage[0], dataRecord, dataLength);
            return true;
        case SUPPORTED_DID_2:
            memcpy(&did_storage[16], dataRecord, dataLength);
            return true;
        case SUPPORTED_DID_3:
            memcpy(&did_storage[32], dataRecord, dataLength);
            return true;
        default:
            return false;
    }
}

// Function to send a positive response after write
void send_positive_response_write_data_by_identifier(uint16_t dataIdentifier) {
    uint8_t response[3];
    response[0] = UDS_WRITE_DATA_BY_IDENTIFIER + 0x40; // Positive response SID (0x2E + 0x40 = 0x6E)
    response[1] = (dataIdentifier >> 8) & 0xFF;       // MSB byte of the dataIdentifier
    response[2] = dataIdentifier & 0xFF;              // LSB byte of the dataIdentifier
    send_can_message(response, 3);
    //send_uart_message(response, 3);
}

// Function to send a negative response for WriteDataByIdentifier
void send_negative_response_write_data_by_identifier(uint8_t nrc) {
    uint8_t response[3];
    response[0] = UDS_NEGATIVE_RESPONSE;   // SID for a negative response
    response[1] = UDS_WRITE_DATA_BY_IDENTIFIER; // Service SID WriteDataByIdentifier (0x2E)
    response[2] = nrc;                     // NRC code
    send_can_message(response, 3);
    //send_uart_message(response, 3);
}

// Function to check specific conditions (example: session state)
bool are_conditions_correct_for_did(uint16_t dataIdentifier) {
    // Implement additional checks if necessary
    // Example: authorization only in an extended session
    if (uds_session.current_session == 0) {  // Default session
        return false;  // Conditions not met if session is default
    }
    return true;
}

void send_can_message(uint8_t *message, uint8_t length) {
    CAN_TxHeaderTypeDef TxHeader;
    uint32_t TxMailbox;

    TxHeader.DLC = length;
    TxHeader.StdId = 0x7E0; // CAN ID for ECU responses
    TxHeader.IDE = CAN_ID_STD;
    TxHeader.RTR = CAN_RTR_DATA;
    TxHeader.TransmitGlobalTime = DISABLE;

    if (HAL_CAN_GetTxMailboxesFreeLevel(&hcan1) == 0) {
         UART_Send("No free mailbox!\n");
        return; // No free mailbox — drop the response rather than blocking
    }

    if(HAL_OK == HAL_CAN_AddTxMessage(&hcan1, &TxHeader, message, &TxMailbox)) {
        // Message sent successfully
        UART_Send("Message sent!\n");
    }
}
