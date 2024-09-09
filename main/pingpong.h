#ifndef PINGPONG_H
#define PINGPONG_H

#include <RadioLib.h>
#include <SPI.h>

#ifndef _PINNUM
#define _PINNUM(port, pin)    ((port)*32 + (pin))
#endif

// Uncomment the following only on one
// of the nodes to initiate the pings
// #define INITIATING_NODE

// External variables
extern SX1262* p_radio;
extern SPIClass* p_rfPort;
extern int transmissionState;
extern bool p_transmitFlag;
extern volatile bool p_operationDone;

// Function declarations
void p_setFlag(void);
void setupPingPong();
void pingpongLoop();
void pingpongStart();

#endif // PINGPONG_H
