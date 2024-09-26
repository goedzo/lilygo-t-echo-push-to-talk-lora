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
      testCounter(0),// Initialize packetCounter to 0
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
    if (bufferSize < 3) {  // Ensure there's enough room for the header
        type = "NULL";  // Invalidate this packet
        return false;  // Packet is too short
    }

    // Debug: Print the received packet as characters
    Serial.print("Received Packet: ");
    for (uint16_t i = 0; i < bufferSize; i++) {
        Serial.print((char)buffer[i]);
    }
    Serial.println();

    length = bufferSize;

    // Directly check if the packet is "Ping!"
    if (strncmp((char*)buffer, "Ping!", bufferSize) == 0) {  // Exclude the message counter from the check
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

    // Handle REQ type
    if (type == "REQ") {
        // The content will be the requested packet counter (a number)
        content = String((char*)(buffer + 3));  // After "REQ"
        Serial.print(F("Requested packet counter: "));
        Serial.println(content);
        return true;
    }


    // Check if the packet is a "MAP" packet and validate the checksum
    if (type == "MAP") {
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
        return true;  // Valid MAP packet
    }

    // Locate the "~~" marker which indicates the start of the content
    uint16_t index = 0;
    while (index < bufferSize) {
        if (buffer[index] == '~' && buffer[index + 1] == '~') {
            // Move past the "~~" marker to get to the content
            index += 2;
            break;
        }
        index++;
    }

    // Extract content for other types after the "~~" marker
    if (index < bufferSize) {
        content = String((char*)(buffer + index));
        Serial.print(F("Content determined: "));
        Serial.println(content);
    } else {
        // Handle the case where there's not enough data for content
        content = String("");
    }

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


bool Packet::parseHeader(uint8_t* buffer, uint16_t bufferSize) {
    if (bufferSize < 3) {  // Ensure there's enough room for at least the type
        Serial.println(F("Packet too short, invalid."));
        return false;
    }

    // Extract the first part of the header up to the first "~"
    uint16_t index = 0;
    while (index < bufferSize && buffer[index] != '~') {
        index++;
    }

    if (index >= bufferSize) {
        Serial.println(F("No separator '~' found, invalid header."));
        return false;  // No separator "~" found
    }

    // Check if the first part of the header matches the type "PT", "RN", or "TX" (with a channel)
    if (strncmp((char*)buffer, "PT", 2) == 0 && index == 3) {
        type = "PTT";  // Set type to "PTT"
        channel = buffer[2];  // Set the channel (PTA, PTB, etc.)
        Serial.print(F("Type determined: PTT on channel "));
        Serial.println(channel);
    } else if (strncmp((char*)buffer, "RN", 2) == 0 && index == 3) {
        type = "RANGE";  // Set type to "RANGE"
        channel = buffer[2];  // Set the channel (e.g., A, B, C, etc.)
        Serial.print(F("Type determined: RANGE on channel "));
        Serial.println(channel);
    } else if (strncmp((char*)buffer, "TX", 2) == 0 && index == 3) {
        type = "TXT";  // Set type to "TXT"
        channel = buffer[2];  // Set the channel (TXA, TXB, etc.)
        Serial.print(F("Type determined: TXT on channel "));
        Serial.println(channel);
    } else if (strncmp((char*)buffer, "MAP", 3) == 0 && index == 3) {
        type = "MAP";
        Serial.println(F("Type determined: MAP"));
    } else if (strncmp((char*)buffer, "REQ", 3) == 0 && index == 3) {
        type = "REQ";
        Serial.println(F("Type determined: REQ (Retransmit Request)"));
    } else {
        type = "NULL";  // Unknown type, mark as invalid
        Serial.println(F("Unknown type, invalid packet."));
        return false;
    }

    // Now process additional headers such as PC, SD (send date), and GP (GPS data)
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

            Serial.print(F("Field type extracted: "));
            Serial.println(fieldType);

            // Process the fields: PC (packet counter), SD (Send DateTime), GP (GPS Data)
            if (strcmp(fieldType, "PC") == 0) {
                // Packet Counter (PC)
                if (fieldLength > 0) {
                    char packetCounterStr[fieldLength + 1];
                    strncpy(packetCounterStr, (char*)(buffer + fieldStart), fieldLength);
                    packetCounterStr[fieldLength] = '\0';  // Null-terminate the string
                    packetCounter = atoi(packetCounterStr);  // Convert to integer
                    Serial.print(F("Packet counter (PC) determined: "));
                    Serial.println(packetCounter);
                }
            } else if (strcmp(fieldType, "SD") == 0) {
                // Send DateTime (SD)
                if (fieldLength == 14) {  // Check for 14 characters for YYYYMMDDHHMMSS
                    char dateTimeStr[15];  // 14 characters + null terminator
                    strncpy(dateTimeStr, (char*)(buffer + fieldStart), 14);
                    dateTimeStr[14] = '\0';
                    sendDateTime = String(dateTimeStr);  // Store as string or convert to actual time format if needed
                    Serial.print(F("Send DateTime (SD) determined: "));
                    Serial.println(sendDateTime);
                }
            } else if (strcmp(fieldType, "GP") == 0) {
                // GPS data (GP)
                if (fieldLength > 0) {  // Ensure some data is available for GPS
                    char gpsStr[fieldLength + 1];  // +1 for null terminator
                    strncpy(gpsStr, (char*)(buffer + fieldStart), fieldLength);
                    gpsStr[fieldLength] = '\0';  // Null-terminate the string
                    gpsData = String(gpsStr);  // Store as string or parse as needed
                    Serial.print(F("GPS data (GP) determined: "));
                    Serial.println(gpsData);
                }
            } else {
                // Unknown field type, handle accordingly
                Serial.print(F("Unknown field: "));
                Serial.println(fieldType);
            }
        }
    }

    return true;
}

