#include "packet.h"
#include <cstring>  // For strncpy and memcpy

#include "lora.h"


Packet::Packet() : type("NULL"), header(""), length(0), content(""), raw(nullptr), rawLength(0), channel('\0'), testCounter(0), packetCounter(0) {}

Packet::~Packet() {
    // Free allocated memory for raw buffer
    if (raw != nullptr) {
        delete[] raw;
    }
}

bool Packet::parsePacket(uint8_t* buffer, uint16_t bufferSize) {
    if (bufferSize < 7) {  // Ensure there's enough room for header and message counter
        type = "NULL";  // Invalidate this packet
        return false;  // Packet is too short
    }

    length = bufferSize;

    // Extract the last 4 characters as the message counter
    char packetCounterStr[5];  // 4 digits + null terminator
    strncpy(packetCounterStr, (char*)(buffer + bufferSize - 4), 4);  // Copy the last 4 characters
    packetCounterStr[4] = '\0';  // Null-terminate the string

    // Validate if the last 4 characters are digits
    bool isValidCounter = true;
    for (int i = 0; i < 4; i++) {
        if (!isdigit(packetCounterStr[i])) {  // If any character is not a digit, mark the counter as invalid
            isValidCounter = false;
            break;
        }
    }

    Serial.print(F("Packet received STR: "));
    Serial.println(packetCounterStr);



    // If the counter is valid, convert it to an integer and remove it from the buffer
    if (isValidCounter) {
        packetCounter = atoi(packetCounterStr);  // Convert the string to an integer
        bufferSize -= 4;  // Adjust the buffer size to remove the counter
    } else {
        packetCounter = -1;  // Set an invalid packetCounter value to indicate no valid counter
    }

    Serial.print(F("Packet received "));
    Serial.println(packetCounter);

    // Directly check if the packet is "Ping!"
    if (strncmp((char*)buffer, "Ping!", bufferSize) == 0) {  // Exclude the message counter from the check
        type = "PING";
        content = "Ping!";
        return true;
    }

    // Parse the header if it's not a "Ping!" message
    if (!parseHeader(buffer, bufferSize)) {
        // If the header is unknown, store the raw message and set type to "NULL"
        rawLength = bufferSize-4;
        raw = new uint8_t[rawLength];  // Allocate memory for raw buffer
        memcpy(raw, buffer, rawLength);  // Copy raw buffer content
        return true;
    }

    // Check if the packet is a "MAP" packet
    if (type == "MAP") {
        // Validate checksum only for MAP packets
        if (bufferSize < 4) {  // Ensure room for header and checksum
            type = "NULL";  // Invalidate this packet
            return false;  // Not enough room for a valid MAP packet with checksum
        }

        unsigned char receivedChecksum = buffer[bufferSize - 1];  // Checksum is before the message counter
        unsigned char calculatedChecksum = calculateChecksum(buffer, bufferSize - 1);  // Exclude the checksum byte

        if (receivedChecksum != calculatedChecksum) {
            Serial.println("Invalid checksum for MAP packet, discarding");
            type = "NULL";  // Invalidate this packet
            return false;  // Invalid MAP packet, discard
        }
        return true;
    }

    // Extract the content for other messages (without checksum validation)
    if (packetCounter == -1) {
        // If no valid counter, extract content starting from position 3 (after the header)
        content = String((char*)(buffer + 3));
    } else {
        // If a valid counter was found, extract the content excluding the counter
        if (bufferSize > 3) {
            // Create a temporary buffer to hold the content without the header and counter
            char temp[bufferSize - 3 + 1];  // +1 for null terminator
            memcpy(temp, buffer + 3, bufferSize - 3);
            temp[bufferSize - 3] = '\0';  // Null-terminate the string

            // Now create the content string
            content = String(temp);
        } else {
            // Handle the case where there's not enough data for content
            content = String("");
        }
    }
    Serial.print(F("content: "));
    Serial.println(content);



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
