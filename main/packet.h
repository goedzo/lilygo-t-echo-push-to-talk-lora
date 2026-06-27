#ifndef PACKET_H
#define PACKET_H

#include <Arduino.h>

class Packet {
public:
    // Properties
    String type;
    String header;
    uint16_t length;
    String content;
    uint8_t* raw;        // Raw message buffer
    uint16_t rawLength;  // Length of the raw message
    char channel;        // Store the channel
    uint32_t packetCounter;  // Message counter to track duplicates or for other purposes
    uint32_t testCounter;  // Message counter to track duplicates or for other purposes
    String gpsData;     // GPS data
    String sendDateTime;// Send date and time

    // Beacon-specific fields (populated when type == "BEACON")
    double  beacon_lat;      // Latitude from ~GP field
    double  beacon_lon;      // Longitude from ~GP field
    uint8_t beacon_battery;  // Battery from ~BT field
    String  beacon_deviceId; // Device ID from B prefix

    // Constructor
    Packet();
    ~Packet();  // Destructor to free raw memory

    // Method to parse raw buffer
    bool parsePacket(uint8_t* buffer, uint16_t bufferSize);

    // Check if the packet is a valid test message
    bool isTestMessage() const;

    // Check if the packet is a TXT message
    bool isTxtMessage() const;

    // Check if the packet is a valid range message
    bool isRangeMessage() const;

    // Check if the packet is a peer beacon
    bool isBeaconPacket() const;

    // Check if the packet is a probe discovery packet
    bool isProbePacket() const;

    bool isTimeOutOfSync();

private:
    bool parseHeader(uint8_t* buffer, uint16_t bufferSize);
};

#endif
