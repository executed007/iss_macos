// The thermal warning fires once per upward transition into serious or critical.
#include <cassert>
#include <cstdio>
#include "core/libraries/videoout/thermal_warn.h"

int main() {
    long last = 0;
    assert(!ThermalWarn(0, last));
    assert(ThermalWarn(2, last));   // nominal -> serious
    assert(!ThermalWarn(2, last));  // no repeat
    assert(ThermalWarn(3, last));   // serious -> critical
    assert(!ThermalWarn(1, last));  // drop to fair
    assert(ThermalWarn(2, last));   // rise re-arms
    last = 1;
    assert(!ThermalWarn(0, last));  // fair -> nominal
    std::puts("thermal-warn-test: ok");
}
