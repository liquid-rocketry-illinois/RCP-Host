#include "RCP_Host/RCP_Host.h"

#include <stdlib.h>
#include <string.h>

/*
 * Status codes:
 *  - -1: Initialization error/RCP not initialized
 *  - -2: IO Error
 *  - -3: Function specific error, documented per function
 *  - 0: Success
 *  - Positive integers: success with info, documented per function
 */


// Utility
static float toFloat(const uint8_t* start) {
    return ((float*)start)[0];
}

static RCP_Channel channel = RCP_CH_ZERO;
static struct RCP_LibInitData* callbacks = NULL;

// The only initialization that needs to happen is saving the callbacks struct
int RCP_init(const struct RCP_LibInitData _callbacks) {
    if(callbacks != NULL)
        return -1;
    callbacks = malloc(sizeof(struct RCP_LibInitData));
    *callbacks = _callbacks;
    return 0;
}

int RCP_isOpen(void) {
    return callbacks != NULL;
}

int RCP_shutdown(void) {
    if(callbacks == NULL)
        return -1;
    free(callbacks);
    callbacks = NULL;
    return 0;
}

void RCP_setChannel(RCP_Channel ch) {
    channel = ch;
}

RCP_Channel RCP_getChannel(void) {
    return channel;
}

// The primary function that gets called periodically by the main application. Will read data from the buffer and
// parse any received RCP messages
// Can return 1 to indicate successful read of a wrong-channel packet
int RCP_poll(void) {
    if(callbacks == NULL)
        return -1;
    uint8_t buffer[66] = {0};

    // Read in just the header byte to get the length of the packet
    size_t bread = callbacks->readData(buffer, 1);
    if(bread != 1)
        return -2;

    // Extract packet length from the header
    uint8_t pktlen = buffer[0] & ~RCP_CHANNEL_MASK;

    // Need to read pktlen + 1 bytes in order to read the device class byte too
    bread = callbacks->readData(buffer + 1, pktlen + 1);
    if(bread != (size_t) pktlen + 1)
        return -2;

    // Check channel after reading from buffer so packets not belonging to this channel are not clogging the buffer
    if((buffer[0] & RCP_CHANNEL_MASK) != channel)
        return 1;

    // Extract timestamp
    uint32_t timestamp = pktlen > 4 ? (buffer[2] << 24) | (buffer[3] << 16) | (buffer[4] << 8) | buffer[5] : 0;
    uint8_t ID = pktlen > 5 ? buffer[6] : 0;

    // Simply follows data format in RCP.md and invokes the appropriate callbacks
    switch(buffer[1]) {
    case RCP_DEVCLASS_TEST_STATE: {
        struct RCP_TestData d = {
            .timestamp = timestamp,
            .dataStreaming = buffer[6] & RCP_DATA_STREAM_MASK,
            .state = buffer[6] & RCP_TEST_STATE_MASK,
            .isInited = buffer[6] & RCP_DEVICE_INITED_MASK,
            .heartbeatTime = buffer[6] & RCP_HEARTBEAT_TIME_MASK
        };

        callbacks->processTestUpdate(d);
        break;
    }

    case RCP_DEVCLASS_SIMPLE_ACTUATOR: {
        struct RCP_SimpleActuatorData d = {
            .timestamp = timestamp,
            .state = buffer[7] ? RCP_SIMPLE_ACTUATOR_ON : RCP_SIMPLE_ACTUATOR_OFF,
            .ID = ID
        };

        callbacks->processSimpleActuatorData(d);
        break;
    }

    case RCP_DEVCLASS_PROMPT: {
        if(buffer[2] == RCP_PromptDataType_RESET) {
            struct RCP_PromptInputRequest req = {
                .type = RCP_PromptDataType_RESET,
                .prompt = NULL
            };
            callbacks->processPromptInput(req);
            break;
        }

        struct RCP_PromptInputRequest req = {
            .type = buffer[2],
            .prompt = (char*) buffer + 3
        };

        callbacks->processPromptInput(req);
        break;
    }

    case RCP_DEVCLASS_CUSTOM: {
        struct RCP_CustomData d = {
            .length = pktlen,
            .data = buffer + 2
        };

        callbacks->processSerialData(d);
        break;
    }

    case RCP_DEVCLASS_ANGLED_ACTUATOR:
    case RCP_DEVCLASS_AM_PRESSURE:
    case RCP_DEVCLASS_AM_TEMPERATURE:
    case RCP_DEVCLASS_PRESSURE_TRANSDUCER:
    case RCP_DEVCLASS_RELATIVE_HYGROMETER:
    case RCP_DEVCLASS_LOAD_CELL: {
        struct RCP_OneFloat d = {
            .devclass = buffer[1],
            .timestamp = timestamp,
            .ID = ID,
            .data = toFloat(buffer + 7)
        };

        callbacks->processOneFloat(d);
        break;
    }

    case RCP_DEVCLASS_BOOL_SENSOR: {
        struct RCP_BoolData d = {
            .timestamp = timestamp,
            .ID = ID,
            .data = buffer[7]
        };

        callbacks->processBoolData(d);
        break;
    }

    case RCP_DEVCLASS_STEPPER:
    case RCP_DEVCLASS_POWERMON: {
        struct RCP_TwoFloat d = {
            .devclass = buffer[1],
            .timestamp = timestamp,
            .ID = ID
        };

        memcpy(d.data, buffer + 7, 8);

        callbacks->processTwoFloat(d);
        break;
    }

    case RCP_DEVCLASS_ACCELEROMETER:
    case RCP_DEVCLASS_GYROSCOPE:
    case RCP_DEVCLASS_MAGNETOMETER: {
        struct RCP_ThreeFloat d = {
            .devclass = buffer[1],
            .timestamp = timestamp,
            .ID = ID
        };

        memcpy(d.data, buffer + 7, 12);

        callbacks->processThreeFloat(d);
        break;
    }

    case RCP_DEVCLASS_GPS: {
        struct RCP_FourFloat d = {
            .devclass = buffer[1],
            .timestamp = timestamp,
            .ID = ID
        };

        memcpy(d.data, buffer + 7, 16);

        callbacks->processFourFloat(d);
        break;
    }

    default:
        break;
    }

    return 0;
}

int RCP_sendEStop(void) {
    if(callbacks == NULL)
        return -1;
    uint8_t ESTOP = channel | 0x00;
    return callbacks->sendData(&ESTOP, 1) == 1 ? 0 : -2;
}


// Most of the testing command packets follow the same format, so they have been moved to a common function
static int RCP__sendTestUpdate(RCP_TestStateControlMode mode, uint8_t param) {
    if(callbacks == NULL)
        return -1;
    uint8_t buffer[3] = {0};
    buffer[0] = channel | 0x01;
    buffer[1] = RCP_DEVCLASS_TEST_STATE;
    buffer[2] = mode | param;
    return callbacks->sendData(buffer, 3) == 3 ? 0 : -2;
}

int RCP_sendHeartbeat(void) {
    return RCP__sendTestUpdate(RCP_HEARTBEATS_CONTROL, 0x0F);
}

int RCP_startTest(uint8_t testnum) {
    return RCP__sendTestUpdate(RCP_TEST_START, testnum);
}

int RCP_stopTest(void) {
    return RCP__sendTestUpdate(RCP_TEST_STOP, 0);
}

int RCP_pauseUnpauseTest(void) {
    return RCP__sendTestUpdate(RCP_TEST_PAUSE, 0);
}

int RCP_deviceReset(void) {
    return RCP__sendTestUpdate(RCP_DEVICE_RESET, 0);
}

int RCP_deviceTimeReset(void) {
    return RCP__sendTestUpdate(RCP_DEVICE_RESET_TIME, 0);
}

int RCP_setDataStreaming(int datastreaming) {
    return RCP__sendTestUpdate(datastreaming ? RCP_DATA_STREAM_START : RCP_DATA_STREAM_STOP, 0);
}

int RCP_setHeartbeatTime(uint8_t heartbeatTime) {
    return RCP__sendTestUpdate(RCP_HEARTBEATS_CONTROL, (heartbeatTime & 0x0F));
}

int RCP_requestTestState(void) {
    return RCP__sendTestUpdate(RCP_TEST_QUERY, 0);
}

int RCP_sendSimpleActuatorWrite(uint8_t ID, RCP_SimpleActuatorState state) {
    if(callbacks == NULL)
        return -1;
    uint8_t buffer[4] = {0};
    buffer[0] = channel | 0x02;
    buffer[1] = RCP_DEVCLASS_SIMPLE_ACTUATOR;
    buffer[2] = ID;
    buffer[3] = state;
    return callbacks->sendData(buffer, 4) == 4 ? 0 : -2;
}

int RCP_sendStepperWrite(uint8_t ID, RCP_StepperControlMode mode, float value) {
    if(callbacks == NULL)
        return -1;
    uint8_t buffer[8] = {0};
    buffer[0] = channel | 6;
    buffer[1] = RCP_DEVCLASS_STEPPER;
    buffer[2] = ID;
    buffer[3] = mode;
    memcpy(buffer + 4, &value, 4);
    return callbacks->sendData(buffer, 8) == 8 ? 0 : -2;
}

int RCP_sendAngledActuatorWrite(uint8_t ID, float value) {
    if(callbacks == NULL) return -1;
    uint8_t buffer[7] = {0};
    buffer[0] = channel | 0x05;
    buffer[1] = RCP_DEVCLASS_ANGLED_ACTUATOR;
    buffer[2] = ID;
    memcpy(buffer + 3, &value, 4);
    return callbacks->sendData(buffer, 7) == 7 ? 0 : -2;
}

// One shot read request to a device with an ID
int RCP_requestGeneralRead(RCP_DeviceClass device, uint8_t ID) {
    if(callbacks == NULL)
        return -1;

    if(device == RCP_DEVCLASS_PROMPT) return -1;
    if(device == RCP_DEVCLASS_TEST_STATE) return RCP_requestTestState();
    uint8_t buffer[3] = {0};
    buffer[0] = channel | 0x01;
    buffer[1] = device;
    buffer[2] = ID;
    return callbacks->sendData(buffer, 3) == 3 ? 0 : -2;
}

// Returns -3 if requested device does not (per RCP standard) support tare requests
int RCP_requestTareConfiguration(RCP_DeviceClass device, uint8_t ID, uint8_t dataChannel, float value) {
    if(callbacks == NULL)
        return -1;
    if(device <= 0x80)
        return -3;

    uint8_t buffer[8] = {0};
    buffer[0] = channel | 6;
    buffer[1] = device;
    buffer[2] = ID;
    buffer[3] = dataChannel;
    memcpy(buffer + 4, &value, 4);
    return callbacks->sendData(buffer, 8) == 8 ? 0 : -2;
}

int RCP_promptRespondGONOGO(RCP_GONOGO gonogo) {
    if(callbacks == NULL)
        return -1;
    uint8_t buffer[3] = {0};
    buffer[0] = channel | 0x01;
    buffer[1] = RCP_DEVCLASS_PROMPT;
    buffer[2] = gonogo;
    return callbacks->sendData(buffer, 3) == 3 ? 0 : -2;
}

int RCP_promptRespondFloat(float value) {
    if(callbacks == NULL)
        return -1;
    uint8_t buffer[6] = {0};
    buffer[0] = channel | 0x04;
    buffer[1] = RCP_DEVCLASS_PROMPT;
    memcpy(buffer + 2, &value, 4);
    return callbacks->sendData(buffer, 6) == 6 ? 0 : -2;
}

// Sends a raw array of bytes to the custom device class
// Returns -3 if requested size is larger than the maximum RCP packet size
int RCP_sendRawSerial(const uint8_t* data, uint8_t size) {
    if(callbacks == NULL)
        return -1;
    if(size > 63 || size == 0)
        return -3;
    uint8_t buffer[65];
    buffer[0] = channel | size;
    buffer[1] = RCP_DEVCLASS_CUSTOM;
    memcpy(buffer + 2, data, size);
    return callbacks->sendData(buffer, size + 2) == (size_t) size + 2 ? 0 : -2;
}
