#include "Bridge.h"

int main(){
    int result = initBridgeAndConnect();
    if(result == BRIDGE_CONNECTED) {
        addTracker(1, "human://WAIST", WAIST);

        sendTrackerStatus(1, messages::TrackerStatus_Status_OK, messages::TrackerStatus_Confidence_HIGH);

        auto start = std::chrono::high_resolution_clock::now();
        while(true){
            auto now = std::chrono::high_resolution_clock::now();
            std::chrono::duration<double> diff = now - start;

            float pos_x = (float)sin(diff.count());
            float pos_y = 1.0f;
            float pos_z = (float)cos(diff.count());

            printf("%.2f %.2f %.2f\n", pos_x, pos_y, pos_z);

            if(!sendTrackerPosition(1, pos_x, pos_y, pos_z, 0.0f, 0.0f, 0.0f, 1.0f)){
                printf("Failed to send tracker position\n");
                break;
            }

            flushIncomingMessages();
            usleep(1000);
        }
    }
    return 0;
}
