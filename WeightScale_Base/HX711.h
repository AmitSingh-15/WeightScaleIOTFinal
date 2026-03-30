#pragma once

// HX711 Load Cell Amplifier Library Stub
// This is a placeholder stub for the HX711 library

class HX711 {
public:
    HX711() {}
    void begin(int DATA, int CLK) {}
    void set_scale(float scale) {}
    void tare() {}
    float get_units(byte times = 1) { return 0.0f; }
    long read_average(byte times = 8) { return 0; }
    long read() { return 0; }
    void power_down() {}
    void power_up() {}
    void set_gain(byte gain) {}
    bool is_ready() { return true; }
    
private:
};
