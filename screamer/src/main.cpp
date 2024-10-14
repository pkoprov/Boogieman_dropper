#include <Arduino.h>
#include "setup.h"
#include "clsScreamer.h"

clsScreamer screamer;

void setup() {
    setupDevice();
    screamer.setupScreamer();
}

void loop() {
    screamer.handleScreamer();
}
