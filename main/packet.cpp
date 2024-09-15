#include "packet.h"
#include <cstring>  // For strncpy and memcpy

#include "lora.h"

Packet::Packet() : type("NULL"), header(""), length(0), content(""), raw(nullptr), rawLength(0), channel('\0'), testCounter(0) {}

Packet::~Packet() {
    // Free allocated memory for raw buffer
    if (raw != nullptr) {
        delete[] raw;
    }
}

bool Packet::parsePacket(uint8_t* buffer, uint16_t bufferSize) {
    if (bufferSize < 3) {  // Ensure there's enough room for header
        type = "NULL"; //Invalidate this packet
        return false;  // Packet is too short
    }

    length = bufferSize;

    // Directly check if the packet is "Ping!"
    if (strncmp((char*)buffer, "Ping!", bufferSize) == 0) {
        type = "PING";
        content = "Ping!";
        return true;
    }

    // Parse the header if it's not a "Ping!" message
    if (!parseHeader(buffer, bufferSize)) {
        // If the header is unknown, store the raw message and set type to "NULL"
        rawLength = bufferSize;
        raw = new uint8_t[rawLength];  // Allocate memory for raw buffer
        memcpy(raw, buffer, rawLength);  // Copy raw buffer content
        return true;
    }

    // Check if the packet is a "MAP" packet
    if (type == "MAP") {
        // Validate checksum only for MAP packets
        if (bufferSize < 4) {
            type = "NULL"; //Invalidate this packet
            return false;  // Not enough room for a valid MAP packet with checksum
        }

        unsigned char receivedChecksum = buffer[bufferSize - 1];  // Last byte is the checksum
        unsigned char calculatedChecksum = calculateChecksum(buffer, bufferSize - 1);  // Exclude the checksum byte itself

        if (receivedChecksum != calculatedChecksum) {
            Serial.println("Invalid checksum for MAP packet, discarding");
            type = "NULL"; //Invalidate this packet
            return false;  // Invalid MAP packet, discard
        }
        return true;
    }

    // Extract the content for other messages (without checksum validation)
    content = String((char*)(buffer + 3));

    // Check if this is a test message and extract the test counter
    if (isTestMessage() || isRangeMessage()) {
        const char* testCounterStr = content.c_str() + 4;
        testCounter = atoi(testCounterStr);
    } else {
        testCounter = 0;
    }

    return true;
}


bool Packet::isTestMessage() const {
    return type.startsWith("TX") && content.startsWith("test");
}

bool Packet::isRangeMessage() const {
    return type == "RANGE" && content.startsWith("test");
}


bool Packet::parseHeader(const uint8_t* buffer, uint16_t bufferSize) {
    if (bufferSize < 3) {
        return false;
    }

    // Extract the first 2 characters for the header
    char headerBuffer[3];
    strncpy(headerBuffer, (char*)buffer, 2);  // Only copy the first 2 characters
    headerBuffer[2] = '\0';  // Null-terminate the header

    header = String(headerBuffer);

    // Extract the 3rd character as the channel
    channel = buffer[2];

    // Check if the header is "PT", "TX", "RN", or "MAP"
    if (header == "PT") {
        type = "PTT";
    } else if (header == "TX") {
        type = "TXT";
    } else if (header == "RN") {
        type = "RANGE";
    } else if (header == "MAP") {
        type = "MAP";  // Frequency map packet
    } else {
        type = "NULL";
        return false;
    }

    return true;
}
