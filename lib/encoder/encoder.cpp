#include "encoder.h"

// forward sequence of (A, B): 00 -> 10 -> 11 -> 01 -> 00
void IRAM_ATTR Encoder::handleA(void* arg) {
    Encoder* enc = static_cast<Encoder*>(arg);
    if (digitalRead(enc->pinA_) != digitalRead(enc->pinB_)) {
        enc->count_++;
    } else {
        enc->count_--;
    }
}

void IRAM_ATTR Encoder::handleB(void* arg) {
    Encoder* enc = static_cast<Encoder*>(arg);
    if (digitalRead(enc->pinA_) == digitalRead(enc->pinB_)) {
        enc->count_++;
    } else  {
        enc->count_--;
    }
}
