#include "RCP_Host/RCP_Host.h"

#include <stdlib.h>
#include <string.h>

// Utility
float toFloat(const uint8_t* start) {
    return ((float*) start)[0];
}

RCP_Channel_t channel = RCP_CH_ZERO;
struct RCP_LibInitData* callbacks = NULL;

// The only initialization that needs to happen is saving the callbacks struct
int RCP_init(const struct RCP_LibInitData _callbacks) {
    if(callbacks != NULL) return -1;
    callbacks = malloc(sizeof(struct RCP_LibInitData));
    *callbacks = _callbacks;
    return 0;
}

int RCP_isOpen() {
    return callbacks != NULL;
}

int RCP_shutdown() {
    if(callbacks == NULL) return -1;
    free(callbacks);
    callbacks = NULL;
    return 0;
}

void RCP_setChannel(RCP_Channel_t ch) {
    channel = ch;
}

// The primary function that gets called periodically by the main application. Will read data from the buffer and
// parse any received RCP messages
int RCP_poll() {
    if(callbacks == NULL) return -2;
    uint8_t buffer[64] = {0};

    // Read in just the header byte to get the length of the packet
    int bread = callbacks->readData(buffer, 1);
    if(bread != 1) return -1;

    // Extract packet length from the header
    uint8_t pktlen = buffer[0] & ~RCP_CHANNEL_MASK;

    // Need to read pktlen + 1 bytes in order to read the device class byte too
    bread = callbacks->readData(buffer + 1, pktlen + 1);
    if(bread != pktlen + 1) return -1;

    // Check channel after reading from buffer so packets not belonging to this channel are not clogging the buffer
    if((buffer[0] & RCP_CHANNEL_MASK) != channel) return 0;

    // Extract timestamp
    uint32_t timestamp = (buffer[2] << 24) | (buffer[3] << 16) | (buffer[4] << 8) | buffer[5];
    uint8_t ID = buffer[6];

    // Simply follows data format in RCP.md and invokes the appropriate callbacks
    switch(buffer[1]) {
        case RCP_DEVCLASS_TEST_STATE: {
            struct RCP_TestData d = {
                    .timestamp = timestamp,
                    .dataStreaming = buffer[6] & 0x80,
                    .state = buffer[6] & RCP_TEST_STATE_MASK,
                    .heartbeatTime = buffer[6] & 0x0F
            };

            callbacks->processTestUpdate(d);
            break;
        }

        case RCP_DEVCLASS_SOLENOID: {
            struct RCP_SolenoidData d = {
                    .timestamp = timestamp,
                    .state = buffer[6] & RCP_SOLENOID_STATE_MASK,
                    .ID = buffer[6] & ~RCP_SOLENOID_STATE_MASK
            };

            callbacks->processSolenoidData(d);
            break;
        }

        case RCP_DEVCLASS_CUSTOM: {
            uint8_t* sdata = (uint8_t*) malloc(pktlen);
            memcpy(sdata, buffer + 2, pktlen);
            struct RCP_CustomData d = {
                    .length = pktlen,
                    .data = sdata
            };

            callbacks->processSerialData(d);
            break;
        }

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

int RCP_sendEStop() {
    if(callbacks == NULL) return -2;
    uint8_t ESTOP = channel | 0x00;
    return callbacks->sendData(&ESTOP, 1) == 1 ? 0 : -1;
}


// Most of the testing command packets follow the same format, so they have been moved to a common function
int RCP__sendTestUpdate(RCP_TestStateControlMode_t mode, uint8_t param) {
    if(callbacks == NULL) return -2;
    uint8_t buffer[3] = {0};
    buffer[0] = channel | 0x01;
    buffer[1] = RCP_DEVCLASS_TEST_STATE;
    buffer[2] = mode | param;
    return callbacks->sendData(buffer, 3) == 3 ? 0 : -1;
}

int RCP_sendHeartbeat() {
    return RCP__sendTestUpdate(RCP_HEARTBEATS_CONTROL, 0x0F);
}

int RCP_startTest(uint8_t testnum) {
    return RCP__sendTestUpdate(RCP_TEST_START, testnum);
}

int RCP_setDataStreaming(int datastreaming) {
    return RCP__sendTestUpdate(datastreaming ? RCP_DATA_STREAM_START : RCP_DATA_STREAM_STOP, 0);
}

int RCP_changeTestProgress(RCP_TestStateControlMode_t mode) {
    if(mode != RCP_TEST_STOP && mode != RCP_TEST_PAUSE) return -3;
    return RCP__sendTestUpdate(mode, 0);
}

int RCP_setHeartbeatTime(uint8_t heartbeatTime) {
    return RCP__sendTestUpdate(RCP_HEARTBEATS_CONTROL, (heartbeatTime & 0x0F));
}

int RCP_requestTestState() {
    return RCP__sendTestUpdate(RCP_TEST_QUERY, 0);
}

int RCP_sendSolenoidWrite(uint8_t ID, RCP_SolenoidState_t state) {
    if(callbacks == NULL) return -2;
    uint8_t buffer[3] = {0};
    buffer[0] = channel | 0x01;
    buffer[1] = RCP_DEVCLASS_SOLENOID;
    buffer[2] = (state & RCP_SOLENOID_STATE_MASK) | (ID & 0x3F);
    return callbacks->sendData(buffer, 3) == 3 ? 0 : -1;
}

int RCP_requestSolenoidRead(uint8_t ID) {
    if(callbacks == NULL) return -2;
    uint8_t buffer[3] = {0};
    buffer[0] = channel | 0x01;
    buffer[1] = RCP_DEVCLASS_SOLENOID;
    buffer[2] = RCP_SOLENOID_READ | (ID & 0x3F);
    return callbacks->sendData(buffer, 3) == 3 ? 0 : -1;
}

int RCP_sendStepperWrite(uint8_t ID, RCP_StepperControlMode_t mode, const void* _value) {
    if(callbacks == NULL) return -2;
    uint8_t buffer[7] = {0};
    uint8_t* value = (uint8_t*) _value;
    buffer[0] = channel | 0x05;
    buffer[1] = RCP_DEVCLASS_STEPPER;
    buffer[2] = (mode & RCP_STEPPER_CONTROL_MODE_MASK) | (ID & 0x3F);
    buffer[3] = value[0];
    buffer[4] = value[1];
    buffer[5] = value[2];
    buffer[6] = value[3];
    return callbacks->sendData(buffer, 7) == 7 ? 0 : -1;
}

int RCP_requestStepperRead(uint8_t ID) {
    if(callbacks == NULL) return -2;
    uint8_t buffer[3] = {0};
    buffer[0] = channel | 0x01;
    buffer[1] = RCP_DEVCLASS_STEPPER;
    buffer[2] = RCP_STEPPER_QUERY_STATE | (ID & 0x3F);
    return callbacks->sendData(buffer, 3) == 3 ? 0 : -1;
}

// One shot read request to a device without an ID, just a device class (e.g. ambient temperature sensor)
int RCP_requestDeviceReadNOID(RCP_DeviceClass_t device) {
    return RCP_requestDeviceReadID(device, 0);
}

// One shot read request to a device with an ID
int RCP_requestDeviceReadID(RCP_DeviceClass_t device, uint8_t ID) {
    if(device <= 0x80 || (ID != 0 && device != RCP_DEVCLASS_PRESSURE_TRANSDUCER)) return -3;
    if(callbacks == NULL) return -2;
    uint8_t buffer[3] = {0};
    buffer[0] = channel | 0x01;
    buffer[1] = device;
    buffer[2] = ID;
    return callbacks->sendData(buffer, 3) == 3 ? 0 : -1;
}

// Sends a raw array of bytes to the custom device class
int RCP_sendRawSerial(const uint8_t* data, uint8_t size) {
    if(callbacks == NULL) return -2;
    if(size > 63) return -3;
    uint8_t buffer[65];
    buffer[0] = channel | size;
    buffer[1] = RCP_DEVCLASS_CUSTOM;
    memcpy(buffer + 2, data, size);
    return callbacks->sendData(buffer, size) == 3 ? 0 : -1;
}