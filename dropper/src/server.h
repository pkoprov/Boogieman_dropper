#pragma once
#include <Arduino.h>

// Pass pointer to your autoMode flag
void setupWebServer(bool* autoModePtr);
void webServerLoop();
