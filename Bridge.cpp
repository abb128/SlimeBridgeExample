#include <ratio>
#include <string_view>
#include <memory>
#include <cstdlib>
#include <filesystem>
#include <sys/socket.h>
#include <cmath>
#include <thread>
#include <chrono>

#include "Bridge.h"
#include "ProtobufMessages.pb.h"
#include "unix-sockets.hpp"

#define TMP_DIR "/tmp"
#define SOCKET_NAME "SlimeVRDriver"

namespace fs = std::filesystem;
namespace {

inline constexpr int HEADER_SIZE = 4;
/// @return iterator after header
template <typename TBufIt>
std::optional<TBufIt> WriteHeader(TBufIt bufBegin, int bufSize, int msgSize) {
    const int totalSize = msgSize + HEADER_SIZE; // include header bytes in total size
    if (bufSize < totalSize) return std::nullopt; // header won't fit

    const auto size = static_cast<uint32_t>(totalSize);
    TBufIt it = bufBegin;
    *(it++) = static_cast<uint8_t>(size);
    *(it++) = static_cast<uint8_t>(size >> 8U);
    *(it++) = static_cast<uint8_t>(size >> 16U);
    *(it++) = static_cast<uint8_t>(size >> 24U);
    return it;
}

/// @return iterator after header
template <typename TBufIt>
std::optional<TBufIt> ReadHeader(TBufIt bufBegin, int numBytesRecv, int& outMsgSize) {
    if (numBytesRecv < HEADER_SIZE) return std::nullopt; // header won't fit

    uint32_t size = 0;
    TBufIt it = bufBegin;
    size = static_cast<uint32_t>(*(it++));
    size |= static_cast<uint32_t>(*(it++)) << 8U;
    size |= static_cast<uint32_t>(*(it++)) << 16U;
    size |= static_cast<uint32_t>(*(it++)) << 24U;

    const auto totalSize = static_cast<int>(size);
    if (totalSize < HEADER_SIZE) return std::nullopt;
    outMsgSize = totalSize - HEADER_SIZE;
    return it;
}

BasicLocalClient client;

inline constexpr int BUFFER_SIZE = 1024;
using ByteBuffer = std::array<uint8_t, BUFFER_SIZE>;
ByteBuffer byteBuffer;

}


bool getNextBridgeMessage(messages::ProtobufMessage& message) {
    if (!client.IsOpen()) return false;

    int bytesRecv = 0;
    try {
        bytesRecv = client.RecvOnce(byteBuffer.begin(), HEADER_SIZE);
    } catch (const std::exception& e) {
        fprintf(stderr, "bridge recv error: %s\n", e.what());
        return false;
    }
    if (bytesRecv == 0) return false; // no message waiting

    int msgSize = 0;
    const std::optional msgBeginIt = ReadHeader(byteBuffer.begin(), bytesRecv, msgSize);
    if (!msgBeginIt) {
        fprintf(stderr, "bridge recv error: invalid message header or size");
        return false;
    }
    if (msgSize <= 0) {
        fprintf(stderr, "bridge recv error: empty message");
        return false;
    }
    try {
        if (!client.RecvAll(*msgBeginIt, msgSize)) {
            fprintf(stderr, "bridge recv error: client closed");
            return false;
        }
    } catch (const std::exception& e) {
        fprintf(stderr, "bridge recv error: %s\n", e.what());
        return false;
    }
    if (!message.ParseFromArray(&(**msgBeginIt), msgSize)) {
        fprintf(stderr, "bridge recv error: failed to parse");
        return false;
    }

    return true;
}

bool sendBridgeMessage(messages::ProtobufMessage& message) {
    if (!client.IsOpen()) return false;
    const auto bufBegin = byteBuffer.begin();
    const auto bufferSize = static_cast<int>(std::distance(bufBegin, byteBuffer.end()));
    const auto msgSize = static_cast<int>(message.ByteSizeLong());
    const std::optional msgBeginIt = WriteHeader(bufBegin, bufferSize, msgSize);
    if (!msgBeginIt) {
        fprintf(stderr, "bridge send error: failed to write header\n");
        return false;
    }
    if (!message.SerializeToArray(&(**msgBeginIt), msgSize)) {
        fprintf(stderr, "bridge send error: failed to serialize\n");
        return false;
    }
    int bytesToSend = static_cast<int>(std::distance(bufBegin, *msgBeginIt + msgSize));
    if (bytesToSend <= 0) {
        fprintf(stderr, "bridge send error: empty message\n");
        return false;
    }
    if (bytesToSend > bufferSize) {
        fprintf(stderr, "bridge send error: message too big\n");
        return false;
    }
    try {
        return client.Send(bufBegin, bytesToSend);
    } catch (const std::exception& e) {
        //client.Close();
        fprintf(stderr, "bridge send error: %s\n", e.what());
        return false;
    }
}

#define BRIDGE_CONNECTED 0
#define BRIDGE_DISCONNECTED 1
#define BRIDGE_ERROR 2

int initBridgeAndConnect() {
    std::string path;
    if(const char* ptr = std::getenv("XDG_RUNTIME_DIR")) {
        const fs::path xdg_runtime = ptr;
        path = (xdg_runtime / SOCKET_NAME).native();
    } else {
        fprintf(stderr, "XDG_RUNTIME_DIR is unset, this may not be expected\n");
        path = (fs::path(TMP_DIR) / SOCKET_NAME).native();
    }

    LocalAcceptorSocket sock(path, 16);
    sock.SetBlocking();

    fprintf(stderr, "Waiting to accept a connection at %s...\n", path.c_str());
    while(client.mConnector == std::nullopt) {
        client.mConnector = sock.Accept();
    }
    client.mPoller.AddConnector(client.mConnector.value().GetDescriptor());
    fprintf(stderr, "Connection accepted!\n");
    return BRIDGE_CONNECTED;
}


bool addTracker(int id, const char *name, TrackerRole role) {
    messages::TrackerAdded *trackerAdded = new messages::TrackerAdded;
    trackerAdded->set_tracker_id(id);
    trackerAdded->set_tracker_serial(name);
    trackerAdded->set_tracker_name("External Tracker");
    trackerAdded->set_tracker_role(role);

    messages::ProtobufMessage message;
    message.set_allocated_tracker_added(trackerAdded);

    return sendBridgeMessage(message);
}

bool sendTrackerPosition(int id, float x, float y, float z, float qx, float qy, float qz, float qw) {
    messages::Position *position = new messages::Position;
    position->set_tracker_id(id);

    position->set_x(x);
    position->set_y(y);
    position->set_z(z);

    position->set_qx(qx);
    position->set_qy(qy);
    position->set_qz(qz);
    position->set_qw(qw);

    messages::ProtobufMessage message;
    message.set_allocated_position(position);

    return sendBridgeMessage(message);
}

bool sendTrackerStatus(int id, messages::TrackerStatus_Status status, messages::TrackerStatus_Confidence confidence) {
    messages::TrackerStatus *s = new messages::TrackerStatus;

    s->set_tracker_id(id);
    s->set_status(status);
    s->set_confidence(confidence);

    messages::ProtobufMessage message;
    message.set_allocated_tracker_status(s);

    return sendBridgeMessage(message);
}

void flushIncomingMessages() {
    messages::ProtobufMessage tmp;
    while(getNextBridgeMessage(tmp));
}
