#pragma once
#include <Arduino.h>

struct MoonData {
    double age;
    double illumination;
    String phaseName;
    int imageIndex;
};

bool fetchMoonData(MoonData &data);
String calculateMoonPhaseName(double age, double illumination);
int calculateMoonImageIndex(double age, double illumination);
