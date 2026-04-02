#include "beamweapon.h"
#include "tween.h"


float frequencyVsFrequencyDamageFactor(int beam_frequency, int shield_frequency)
{
    constexpr float pi = 3.14159265358979323846f;

    if (beam_frequency < 0 || shield_frequency < 0)
        return 1.f;

    float diff = static_cast<float>(abs(beam_frequency - shield_frequency));
    float f1 = sinf(Tween<float>::linear(diff, 0.0f, static_cast<float>(BeamWeaponSys::max_frequency), 0.0f, pi * (1.2f + shield_frequency * 0.05f)) + pi / 2.0f);
    f1 = f1 * Tween<float>::easeInCubic(diff, 0, BeamWeaponSys::max_frequency, 1.f, 0.1f);
    f1 = Tween<float>::linear(f1, 1.f, -1.f, 0.5f, 1.5f);
    return f1;
}

string frequencyToString(int frequency)
{
    return string(400 + (frequency * 20)) + "THz";
}
