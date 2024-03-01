#include "ProtobufMessages.pb.h"

enum TrackerRole {
    NONE = 0,
    WAIST = 1,
    LEFT_FOOT = 2,
    RIGHT_FOOT = 3,
    CHEST = 4,
    LEFT_KNEE = 5,
    RIGHT_KNEE = 6,
    LEFT_ELBOW = 7,
    RIGHT_ELBOW = 8,
    LEFT_SHOULDER = 9,
    RIGHT_SHOULDER = 10,
    LEFT_HAND = 11,
    RIGHT_HAND = 12,
    LEFT_CONTROLLER = 13,
    RIGHT_CONTROLLER = 14,
    HEAD = 15,
    NECK = 16,
    CAMERA = 17,
    KEYBOARD = 18,
    HMD = 19,
    BEACON = 20,
    GENERIC_CONTROLLER = 21,
};


#define BRIDGE_CONNECTED 0
#define BRIDGE_DISCONNECTED 1
#define BRIDGE_ERROR 2

int initBridgeAndConnect();
bool addTracker(int id, const char *name, TrackerRole role);
bool sendTrackerPosition(int id, float x, float y, float z, float qx, float qy, float qz, float qw);
bool sendTrackerStatus(int id, messages::TrackerStatus_Status status, messages::TrackerStatus_Confidence confidence);
void flushIncomingMessages();
