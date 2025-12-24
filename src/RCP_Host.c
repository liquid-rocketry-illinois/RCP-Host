// Macroing the staticness of the internals allows for unit testing
#ifdef RCPH_TEST_MODE
#define STATIC
#else
#define STATIC static
#endif

#include "RCP_Host/RCP_Host.h"

#include <stdlib.h>
#include <string.h>

// Stores some basic state
STATIC RCP_Channel channel = RCP_CH_ZERO;
STATIC RCP_PromptDataType activePromptType = RCP_PromptDataType_RESET;

// Callback struct and buffer for storing packet
STATIC struct RCP_LibInitData* callbacks = NULL;
STATIC uint8_t* buffer = NULL;

// String representations of the valid error messages
STATIC char const* const err_msgs[] = {"Success",
                                       "Not Initialized",
                                       "Memory Allocation Error",
                                       "IO Send Error",
                                       "Device Class cannot be used with this function",
                                       "No active prompt",
                                       "IO Receive Error",
                                       "Amalgamation unit nested in another amalgamation unit",
                                       "Invalid amalgamation subunit"};

// Initialize the library by allocated and setting the callbacks struct, allocating the packet buffer, and resetting
// state
RCP_Error RCP_init(const struct RCP_LibInitData _callbacks) {
    if(callbacks != NULL || buffer != NULL) return RCP_ERR_INIT;

    callbacks = malloc(sizeof(struct RCP_LibInitData));
    if(callbacks == NULL) return RCP_ERR_MEMALLOC;
    *callbacks = _callbacks;

    buffer = malloc(RCP_MAX_EXTENDED_BYTES + RCP_MAX_NON_PARAM);
    if(buffer == NULL) return RCP_ERR_MEMALLOC;

    channel = RCP_CH_ZERO;
    activePromptType = RCP_PromptDataType_RESET;

    return RCP_ERR_SUCCESS;
}

// RCP readiness is determined only on whether the callbacks and buffer are initialized
int RCP_isOpen(void) { return callbacks != NULL && buffer != NULL; }

// Deallocate the buffer and callbacks
RCP_Error RCP_shutdown(void) {
    if(callbacks == NULL || buffer == NULL)
        return RCP_ERR_INIT;

    free(callbacks);
    callbacks = NULL;
    free(buffer);
    buffer = NULL;

    return RCP_ERR_SUCCESS;
}

// Return the string representation of an errno
const char* RCP_errstr(RCP_Error rerrno) {
    if(rerrno < 0 || rerrno >= (sizeof(err_msgs) / sizeof(char*))) return NULL;
    return err_msgs[rerrno];
}

// Set the channel
void RCP_setChannel(RCP_Channel ch) { channel = ch; }

// Get the currently set channel
RCP_Channel RCP_getChannel(void) { return channel; }

// Helper for processing an individual information unit. The parameters are a little funky since this also is used to
// process IUs in an amalgamated IU.
// - devclasss: The device class for this IU
// - timestamp: The extracted timestamp from the packet, if relevant. If the timestamp is not used due to the devclass,
//   the value does not matter as it will be ignored
// - params: The number of parameter bytes. Should be set to zero when called from an amalgamated packet to prevent
//   invalid amalgamate subunits
// - postTS: A pointer to the buffer location at the start of the parameter bytes for this IU, but after the timestamp,
//   if there is one
// - inc: A pointer to a size_t that indicates how many parameter bytes were parsed, so that when processing an
//   amalgamated IU, the caller knows how many bytes to move forward
STATIC RCP_Error processIU(RCP_DeviceClass devclass, uint32_t timestamp, uint16_t params, const uint8_t* postTS,
                           size_t* inc) {
    // The value to be assigned to inc, if it is not null at the very end
    size_t incval = 0;

    // Return value
    RCP_Error rerrno;

    switch(devclass) {
    case RCP_DEVCLASS_TEST_STATE: {
        struct RCP_TestData d = {.timestamp = timestamp,
                                 .dataStreaming = postTS[0] & RCP_DATA_STREAM_MASK,
                                 .state = postTS[0] & RCP_TEST_STATE_MASK,
                                 .isInited = postTS[0] & RCP_DEVICE_INITED_MASK,
                                 .heartbeatTime = postTS[1],
                                 .runningTest = 0,
                                 .testProgress = 0};

        // Under most circumstances, incval can be 2 since state will not be RCP_TEST_RUNNING
        incval = 2;

        // If there is a running test, set the correct values
        if(d.state == RCP_TEST_RUNNING) {
            incval = 4;
            d.runningTest = postTS[2];
            d.testProgress = postTS[3];
        }

        rerrno = callbacks->processTestUpdate(d);
        break;
    }

    case RCP_DEVCLASS_SIMPLE_ACTUATOR: {
        struct RCP_SimpleActuatorData d = {.timestamp = timestamp,
                                           .state = postTS[1] ? RCP_SIMPLE_ACTUATOR_ON : RCP_SIMPLE_ACTUATOR_OFF,
                                           .ID = postTS[0]};

        incval = 2;
        rerrno = callbacks->processSimpleActuatorData(d);
        break;
    }

    case RCP_DEVCLASS_PROMPT: {
        // Params will only be zero when this function is called when processing amalgamated subunits. If that is the
        // case and a prompt IU is detected, exit since it is ill-formed
        if(params == 0) return RCP_ERR_AMALG_SUBUNIT;

        if(postTS[0] == RCP_PromptDataType_RESET) {
            struct RCP_PromptInputRequest req = {.type = RCP_PromptDataType_RESET, .prompt = NULL, .length = 0};
            return callbacks->processPromptInput(req);
        }

        // It is up to the callback function to appropriately parse out the number of chars
        struct RCP_PromptInputRequest req = {.type = postTS[0], .prompt = (char*) (postTS + 1), .length = params - 1};
        activePromptType = req.type;

        incval = 0;
        rerrno = callbacks->processPromptInput(req);
        break;
    }

    case RCP_DEVCLASS_TARGET_LOG: {
        // See Prompt IU section
        if(params == 0) return RCP_ERR_AMALG_SUBUNIT;

        struct RCP_TargetLogData d = {.timestamp = timestamp, .data = (char*) postTS, .length = params - 4};

        incval = 0;
        rerrno = callbacks->processTargetLog(d);
        break;
    }

    case RCP_DEVCLASS_ANGLED_ACTUATOR:
    case RCP_DEVCLASS_AM_PRESSURE:
    case RCP_DEVCLASS_TEMPERATURE:
    case RCP_DEVCLASS_PRESSURE_TRANSDUCER:
    case RCP_DEVCLASS_RELATIVE_HYGROMETER:
    case RCP_DEVCLASS_LOAD_CELL: {
        // All the 1F devices
        struct RCP_1F d = {.devclass = devclass, .timestamp = timestamp, .ID = postTS[0]};

        memcpy(&d.data, postTS + 1, 4);

        incval = 5;
        rerrno = callbacks->processOneFloat(d);
        break;
    }

    case RCP_DEVCLASS_BOOL_SENSOR: {
        struct RCP_BoolData d = {.timestamp = timestamp, .ID = postTS[0], .data = postTS[1]};

        incval = 2;
        rerrno = callbacks->processBoolData(d);
        break;
    }

    case RCP_DEVCLASS_STEPPER:
    case RCP_DEVCLASS_POWERMON: {
        // All the 2F devices
        struct RCP_2F d = {.devclass = devclass, .timestamp = timestamp, .ID = postTS[0]};

        memcpy(d.data, postTS + 1, 8);

        incval = 9;
        rerrno = callbacks->processTwoFloat(d);
        break;
    }

    case RCP_DEVCLASS_ACCELEROMETER:
    case RCP_DEVCLASS_GYROSCOPE:
    case RCP_DEVCLASS_MAGNETOMETER: {
        // All the 3F devices
        struct RCP_3F d = {.devclass = devclass, .timestamp = timestamp, .ID = postTS[0]};

        memcpy(d.data, postTS + 1, 12);

        incval = 13;
        rerrno = callbacks->processThreeFloat(d);
        break;
    }

    case RCP_DEVCLASS_GPS: {
        // All the 4F devices
        struct RCP_4F d = {.devclass = devclass, .timestamp = timestamp, .ID = postTS[0]};

        memcpy(d.data, postTS + 1, 16);

        incval = 17;
        rerrno = callbacks->processFourFloat(d);
        break;
    }

    // This function does not process amalgamate IUs. That is up to the caller of this function
    case RCP_DEVCLASS_AMALGAMATE:
        return RCP_ERR_AMALG_NESTING;

    default:
        return RCP_ERR_INVALID_DEVCLASS;
    }

    // Only assign to inc if it is non-null
    if(inc != NULL) *inc = incval;
    return rerrno;
}

RCP_Error RCP_poll(void) {
    // Check init
    if(callbacks == NULL) return RCP_ERR_INIT;

    // Read first byte of packet to determine format
    size_t bread = callbacks->readData(buffer, 1);
    if(bread != 1) return RCP_ERR_IO_RCV;

    // Total parameter bytes (including timestamp)
    uint16_t params = 0;

    // Length of header
    uint8_t preambleLen = 1;

    // Pointer to current location in buffer. Used for amalgamate IUs
    uint8_t* head = NULL;

    // If extended format...
    if(buffer[0] & RCP_EXTENDED_MASK) {
        preambleLen = 3;

        // Read length bytes
        bread = callbacks->readData(buffer + 1, 2);
        if(bread != 2) return RCP_ERR_IO_RCV;

        // Assign length
        params = buffer[1] << 8;
        params |= buffer[2];

        // Do nothing on zero length packets
        if(params == 0) return RCP_ERR_SUCCESS;

        // Read rest of the bytes
        bread = callbacks->readData(buffer + 3, params + 1);
        if(bread != params + 1) return RCP_ERR_IO_RCV;

        // Exit early if wrong channel
        if((buffer[0] & RCP_CHANNEL_MASK) != channel) return RCP_ERR_SUCCESS;

        // Set head to correct location
        head = buffer + 3;
    }

    // If compact...
    else {
        // Determine parameter bytes
        params = buffer[0] & RCP_COMPACT_LENGTH_MASK;

        // Do nothing on zero length packets
        if(params == 0) return RCP_ERR_SUCCESS;

        // Read rest of the bytes
        bread = callbacks->readData(buffer + 1, params + 1);
        if(bread != params + 1) return RCP_ERR_IO_RCV;

        // Exit early if wrong channel
        if((buffer[0] & RCP_CHANNEL_MASK) != channel) return RCP_ERR_SUCCESS;

        // Set head to correct location
        head = buffer + 1;
    }

    // Extract the device classs
    RCP_DeviceClass devclass = *head;
    head++;

    // Extract the timestamp. If the packet doesn't have a timestamp (at the time, only the prompt class), do not assign timestamp and don't increment head
    uint32_t timestamp = 0;
    if(devclass != RCP_DEVCLASS_PROMPT) {
        timestamp = (head[0] << 24) | (head[1] << 16) | head[2] << 8 | head[3];
        head += 4;
    }

    // If not an amalgamate IU, process the IU directly
    if(devclass != RCP_DEVCLASS_AMALGAMATE) return processIU(devclass, timestamp, params, head, NULL);

    // Otherwise, continue looping over subunits until we've gone through all of them
    while(head - (preambleLen + params) != buffer) {
        size_t inc = 0;
        devclass = head[0];
        head++;

        RCP_Error rerrno = processIU(devclass, timestamp, 0, head, &inc);
        if(rerrno != RCP_ERR_SUCCESS) return rerrno;
        head += inc;
    }

    return RCP_ERR_SUCCESS;
}

RCP_Error RCP_sendEStop(void) {
    if(callbacks == NULL) return RCP_ERR_INIT;
    buffer[0] = channel | 0x00;
    return callbacks->sendData(&buffer, 1) == 1 ? RCP_ERR_SUCCESS : RCP_ERR_IO_SEND;
}

// Most of the testing command packets follow the same format, so they have been moved to a common function
STATIC RCP_Error RCP__sendTestUpdate(RCP_TestStateControlMode mode, uint8_t param) {
    if(callbacks == NULL) return RCP_ERR_INIT;
    uint8_t len = 3;

    if(mode == RCP_TEST_START || mode == RCP_HEARTBEATS_CONTROL) {
        len = 4;
        buffer[0] = channel | 0x02;
        buffer[1] = RCP_DEVCLASS_TEST_STATE;
        buffer[2] = mode;
        buffer[3] = param;
    }

    else {
        buffer[0] = channel | 0x01;
        buffer[1] = RCP_DEVCLASS_TEST_STATE;
        buffer[2] = mode;
    }

    return callbacks->sendData(buffer, len) == len ? RCP_ERR_SUCCESS : RCP_ERR_IO_SEND;
}

RCP_Error RCP_sendHeartbeat(void) { return RCP__sendTestUpdate(RCP_HEARTBEAT, 0); }

RCP_Error RCP_startTest(uint8_t testnum) { return RCP__sendTestUpdate(RCP_TEST_START, testnum); }

RCP_Error RCP_stopTest(void) { return RCP__sendTestUpdate(RCP_TEST_STOP, 0); }

RCP_Error RCP_pauseUnpauseTest(void) { return RCP__sendTestUpdate(RCP_TEST_PAUSE, 0); }

RCP_Error RCP_deviceReset(void) { return RCP__sendTestUpdate(RCP_DEVICE_RESET, 0); }

RCP_Error RCP_deviceTimeReset(void) { return RCP__sendTestUpdate(RCP_DEVICE_RESET_TIME, 0); }

RCP_Error RCP_setDataStreaming(int datastreaming) {
    return RCP__sendTestUpdate(datastreaming ? RCP_DATA_STREAM_START : RCP_DATA_STREAM_STOP, 0);
}

RCP_Error RCP_setHeartbeatTime(uint8_t heartbeatTime) {
    return RCP__sendTestUpdate(RCP_HEARTBEATS_CONTROL, heartbeatTime);
}

RCP_Error RCP_requestTestState(void) { return RCP__sendTestUpdate(RCP_TEST_QUERY, 0); }

RCP_Error RCP_sendSimpleActuatorWrite(uint8_t ID, RCP_SimpleActuatorState state) {
    if(callbacks == NULL) return RCP_ERR_INIT;
    buffer[0] = channel | 0x02;
    buffer[1] = RCP_DEVCLASS_SIMPLE_ACTUATOR;
    buffer[2] = ID;
    buffer[3] = state;
    return callbacks->sendData(buffer, 4) == 4 ? RCP_ERR_SUCCESS : RCP_ERR_IO_SEND;
}

RCP_Error RCP_sendStepperWrite(uint8_t ID, RCP_StepperControlMode mode, float value) {
    if(callbacks == NULL) return RCP_ERR_INIT;
    buffer[0] = channel | 6;
    buffer[1] = RCP_DEVCLASS_STEPPER;
    buffer[2] = ID;
    buffer[3] = mode;
    memcpy(buffer + 4, &value, 4);
    return callbacks->sendData(buffer, 8) == 8 ? RCP_ERR_SUCCESS : RCP_ERR_IO_SEND;
}

RCP_Error RCP_sendAngledActuatorWrite(uint8_t ID, float value) {
    if(callbacks == NULL) return RCP_ERR_INIT;
    buffer[0] = channel | 0x05;
    buffer[1] = RCP_DEVCLASS_ANGLED_ACTUATOR;
    buffer[2] = ID;
    memcpy(buffer + 3, &value, 4);
    return callbacks->sendData(buffer, 7) == 7 ? RCP_ERR_SUCCESS : RCP_ERR_IO_SEND;
}

// One shot read request to a device with an ID
RCP_Error RCP_requestGeneralRead(RCP_DeviceClass device, uint8_t ID) {
    if(callbacks == NULL) return RCP_ERR_INIT;

    if(device == RCP_DEVCLASS_PROMPT || device == RCP_DEVCLASS_TARGET_LOG || device == RCP_DEVCLASS_AMALGAMATE)
        return RCP_ERR_INVALID_DEVCLASS;
    if(device == RCP_DEVCLASS_TEST_STATE) return RCP_requestTestState();

    buffer[0] = channel | 0x01;
    buffer[1] = device;
    buffer[2] = ID;
    return callbacks->sendData(buffer, 3) == 3 ? RCP_ERR_SUCCESS : RCP_ERR_IO_SEND;
}

RCP_Error RCP_requestTareConfiguration(RCP_DeviceClass device, uint8_t ID, uint8_t dataChannel, float offset) {
    if(callbacks == NULL) return RCP_ERR_INIT;
    if(device <= 0x80 || device == RCP_DEVCLASS_BOOL_SENSOR || device == RCP_DEVCLASS_AMALGAMATE)
        return RCP_ERR_INVALID_DEVCLASS;

    buffer[0] = channel | 6;
    buffer[1] = device;
    buffer[2] = ID;
    buffer[3] = dataChannel;
    memcpy(buffer + 4, &offset, 4);
    return callbacks->sendData(buffer, 8) == 8 ? RCP_ERR_SUCCESS : RCP_ERR_IO_SEND;
}

RCP_Error RCP_promptRespondGONOGO(RCP_GONOGO gonogo) {
    if(callbacks == NULL) return RCP_ERR_INIT;
    if(activePromptType != RCP_PromptDataType_GONOGO) return RCP_ERR_NO_ACTIVE_PROMPT;

    buffer[0] = channel | 0x01;
    buffer[1] = RCP_DEVCLASS_PROMPT;
    buffer[2] = gonogo;
    return callbacks->sendData(buffer, 3) == 3 ? RCP_ERR_SUCCESS : RCP_ERR_IO_SEND;
}

RCP_Error RCP_promptRespondFloat(float value) {
    if(callbacks == NULL) return RCP_ERR_INIT;
    if(activePromptType != RCP_PromptDataType_Float) return RCP_ERR_NO_ACTIVE_PROMPT;

    buffer[0] = channel | 0x04;
    buffer[1] = RCP_DEVCLASS_PROMPT;
    memcpy(buffer + 2, &value, 4);
    return callbacks->sendData(buffer, 6) == 6 ? RCP_ERR_SUCCESS : RCP_ERR_IO_SEND;
}

RCP_PromptDataType RCP_getActivePromptType(void) { return activePromptType; }
