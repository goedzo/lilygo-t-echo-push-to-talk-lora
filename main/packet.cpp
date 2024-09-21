#include "packet.h"
#include <cstring>  // For strncpy and memcpy

#include "lora.h"


Packet::Packet() 
    : type("NULL"),    // Initialize type to "NULL"
      header(""),      // Initialize header as an empty string
      length(0),       // Initialize length to 0
      content(""),     // Initialize content as an empty string
      raw(nullptr),    // Initialize raw buffer to nullptr
      rawLength(0),    // Initialize raw length to 0
      channel('\0'),   // Initialize channel to null character
      packetCounter(0),// Initialize packetCounter to 0
      gpsData(""),     // Add gpsData as an empty string if you store it
      sendDateTime("") // Add sendDateTime as an empty string
{}

Packet::~Packet() {
    // Free allocated memory for raw buffer
    if (raw != nullptr) {
        delete[] raw;
    }
}

bool Packet::parsePacket(uint8_t* buffer, uint16_t bufferSize) {
    if (bufferSize < 3) {  // Ensure there's enough room for at least the type
        type = "NULL";  // Invalidate this packet
        return false;  // Packet is too short
    }

    length = bufferSize;
    uint16_t index = 0;

    // Extract the 3-character packet type
    char typeBuffer[4];
    strncpy(typeBuffer, (char*)buffer, 3);
    typeBuffer[3] = '\0';  // Null-terminate
    type = String(typeBuffer);
    index += 3;

    // Process the headers until we encounter the "~~" marker
    while (index < bufferSize) {
        if (buffer[index] == '~' && buffer[index + 1] == '~') {
            // Found the end of the headers, move past the "~~" marker
            index += 2;
            break;
        }
        if (buffer[index] != '~') {
            break;  // No more fields if there's no separator
        }
        index++;  // Skip the separator '~'

        // Find the next separator (or end of the string) for this field
        uint16_t fieldStart = index;
        while (index < bufferSize && buffer[index] != '~') {
            index++;
        }
        uint16_t fieldLength = index - fieldStart;

        // If we have found a field, process the field identifier and value
        if (fieldLength >= 2) {
            char fieldType[3];
            strncpy(fieldType, (char*)(buffer + fieldStart), 2);  // Extract the 2-character field type
            fieldType[2] = '\0';

            // Move past the field identifier
            fieldStart += 2;
            fieldLength -= 2;

            // Process each type of field based on the field identifier
            if (strcmp(fieldType, "PC") == 0) {
                // Packet Counter (PC)
                if (fieldLength > 0) {
                    char packetCounterStr[fieldLength + 1];
                    strncpy(packetCounterStr, (char*)(buffer + fieldStart), fieldLength);
                    packetCounterStr[fieldLength] = '\0';  // Null-terminate the string
                    packetCounter = atoi(packetCounterStr);  // Convert to integer
                }
            } else if (strcmp(fieldType, "SD") == 0) {
                // Send DateTime (SD)
                if (fieldLength == 14) {  // Check for 14 characters for YYYYMMDDHHMMSS
                    char dateTimeStr[15];  // 14 characters + null terminator
                    strncpy(dateTimeStr, (char*)(buffer + fieldStart), 14);
                    dateTimeStr[14] = '\0';
                    sendDateTime = String(dateTimeStr);  // Store as string or convert to actual time format if needed
                }
            } else if (strcmp(fieldType, "GP") == 0) {
                // GPS data (GP)
                if (fieldLength > 0) {  // Ensure some data is available for GPS
                    char gpsStr[fieldLength + 1];  // +1 for null terminator
                    strncpy(gpsStr, (char*)(buffer + fieldStart), fieldLength);
                    gpsStr[fieldLength] = '\0';  // Null-terminate the string
                    gpsData = String(gpsStr);  // Store as string or parse as needed
                }
            } else {
                // Unknown field type, handle accordingly
            }
        }
    }

    // Extract the content after the "~~" marker
    if (index < bufferSize) {
        content = String((char*)(buffer + index));
    } else {
        content = "";  // No content
    }

    return true;  // Successfully parsed
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
