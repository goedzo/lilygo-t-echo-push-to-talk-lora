#include "packet.h"
#include <cstring>  // For strncpy and memcpy

Packet::Packet() : type("NULL"), header(""), length(0), content(""), raw(nullptr), rawLength(0), testCounter(0) {}

Packet::~Packet() {
    // Free allocated memory for raw buffer
    if (raw != nullptr) {
        delete[] raw;
    }
}

bool Packet::parsePacket(uint8_t* buffer, uint16_t bufferSize, char channel) {
    if (bufferSize < 3) {
        return false;  // Packet is too short
    }

    length = bufferSize;

    // Generate expected headers based on the single channel string
    String expectedHeaders[2];  // Fixed-size array for headers
    expectedHeaders[0] = String("PT") + String(channel);
    expectedHeaders[1] = String("TX") + String(channel);

    // Parse the header
    if (!parseHeader(buffer, bufferSize, expectedHeaders, 2)) {
        // If the header is unknown, store the raw message and set type to "NULL"
        rawLength = bufferSize;
        raw = new uint8_t[rawLength];  // Allocate memory for raw buffer
        memcpy(raw, buffer, rawLength);  // Copy raw buffer content
        return false;
    }

    // Extract the content
    content = String((char*)(buffer + 3));

    // Check if this is a test message and extract the test counter
    if (isTestMessage()) {
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

bool Packet::parseHeader(const uint8_t* buffer, uint16_t bufferSize, const String expectedHeaders[], uint8_t headerCount) {
    char headerBuffer[4];
    strncpy(headerBuffer, (char*)buffer, 3);  // Extract the first 3 characters
    headerBuffer[3] = '\0';  // Null-terminate the header

    header = String(headerBuffer);

    for (uint8_t i = 0; i < headerCount; i++) {
        Serial.print("Expected header: ");
        Serial.println(expectedHeaders[i]);

        if (header == expectedHeaders[i]) {
            type = expectedHeaders[i].startsWith("PT") ? "PTT" : (expectedHeaders[i].startsWith("TX") ? "TXT" : "RAW");
            return true;
        }
    }

    // If no match found, set type to "NULL"
    type = "NULL";
    return false;
}

