/*
  ==============================================================================
    FDN_DSP.h
    Phase 188: Fix Compilation Errors (ratioSum)

    FIXES:
    - Restored 'ratioSum' calculation in updatePhysics.
    - Ensured robust compilation for SFX logic.
  ==============================================================================
*/

#pragma once
#include <vector>
#include <array>
#include <cmath>
#include <algorithm>
#include <random>
#include <atomic>

// ==============================================================================
// 1. CONSTANTS & UTILITIES
// ==============================================================================

static constexpr float MAX_DELAY_SECONDS = 10.0f;
static constexpr float PI = 3.14159265359f;
static constexpr float PI_2 = 6.28318530718f;
static constexpr float HALF_PI = 1.570796327f;
static constexpr float SPEED_OF_SOUND = 343.0f;
static constexpr int FDN_CHANNELS = 16;
static constexpr float HARD_CLIP_THRESHOLD = 2.0f;

static constexpr float REFERENCE_SAMPLE_RATE = 48000.0f;
static constexpr float STEREO_SPREAD_MS = 0.5f;

static constexpr float LFO_RATIOS[16] = {
    1.000f, 0.618f, 1.272f, 0.786f, 1.618f, 0.382f, 1.414f, 0.528f,
    1.175f, 0.854f, 1.324f, 0.472f, 1.089f, 0.927f, 1.236f, 0.691f
};

// --- Helper Functions ---

inline float antiDenormal(float x) {
    return (std::abs(x) < 1.0e-20f) ? 0.0f : x;
}

inline float hardClip(float x) {
    return std::clamp(x, -HARD_CLIP_THRESHOLD, HARD_CLIP_THRESHOLD);
}

inline float softSaturate(float x, float drive) {
    if (drive < 0.001f) return x;
    float driveAmount = drive * 4.0f;
    float shaped = x * (1.0f + driveAmount);
    float saturated = std::tanh(shaped);
    return saturated / (1.0f + driveAmount);
}

inline float safeLoopSaturate(float x) {
    if (x > 1.5f) return 1.5f + std::tanh(x - 1.5f) * 0.1f;
    if (x < -1.5f) return -1.5f + std::tanh(x + 1.5f) * 0.1f;
    return x;
}

inline int findNearestPrime(int n) {
    auto isPrime = [](int num) {
        if (num <= 1) return false;
        if (num <= 3) return true;
        if (num % 2 == 0 || num % 3 == 0) return false;
        for (int i = 5; i * i <= num; i += 6)
            if (num % i == 0 || num % (i + 2) == 0) return false;
        return true;
        };
    if (n <= 2) return 2;
    int up = n;
    while (!isPrime(up)) up++;
    int down = n;
    while (down > 2 && !isPrime(down)) down--;
    return ((up - n) < (n - down)) ? up : down;
}

static float calcAirAbsorption(float freq, float tempC, float humidity) {
    float safeTemp = std::clamp(tempC, -50.0f, 100.0f);
    float safeHum = std::clamp(humidity, 1.0f, 100.0f);
    float T = safeTemp + 273.15f;
    float T0 = 293.15f;
    float P_atm = 1.0f;
    float h_rel = safeHum;
    float exponent = -6.8346f * std::pow(T0 / T, 1.261f) + 4.6151f;
    float psat_ratio = std::pow(10.0f, exponent);
    float h = h_rel * psat_ratio;
    float fr_O = P_atm * (24.0f + 4.04e4f * h * (0.02f + h) / (0.391f + h));
    float fr_N = P_atm * std::pow(T / T0, -0.5f) * (9.0f + 280.0f * h * std::exp(-4.17f * (std::pow(T / T0, -1.0f / 3.0f) - 1.0f)));
    float f = freq;
    float f2 = f * f;
    float alpha = 8.686f * f2 * std::pow(T, -2.5f) * (
        1.84e-11f * (1.0f / P_atm) * std::sqrt(T / T0) +
        std::pow(T / T0, -2.5f) * (
            0.01275f * std::exp(-2239.1f / T) / (fr_O + f2 / fr_O) +
            0.1068f * std::exp(-3352.0f / T) / (fr_N + f2 / fr_N)
            )
        );
    alpha *= 100.0f;
    if (std::isnan(alpha) || std::isinf(alpha)) return 1.0f;
    return std::pow(10.0f, -alpha / 20.0f);
}

// ==============================================================================
// 2. MATERIAL DATABASE
// ==============================================================================
struct MaterialDef {
    std::array<float, 6> absorption = { 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f };
    bool isResonator = false;
};

namespace MaterialDB {
    inline const MaterialDef& get(int index) {
        static const MaterialDef data[] = {
            // Basic (0-12)
            { { 0.02f, 0.03f, 0.04f, 0.05f, 0.06f, 0.07f }, false },
            { { 0.10f, 0.08f, 0.06f, 0.05f, 0.04f, 0.02f }, false },
            { { 0.15f, 0.13f, 0.11f, 0.10f, 0.08f, 0.07f }, false },
            { { 0.20f, 0.18f, 0.15f, 0.12f, 0.10f, 0.10f }, false },
            { { 0.05f, 0.15f, 0.30f, 0.40f, 0.55f, 0.70f }, false },
            { { 0.15f, 0.35f, 0.45f, 0.55f, 0.70f, 0.85f }, false },
            { { 0.20f, 0.40f, 0.60f, 0.70f, 0.80f, 0.90f }, false },
            { { 0.05f, 0.04f, 0.04f, 0.04f, 0.05f, 0.05f }, false },
            { { 0.30f, 0.20f, 0.15f, 0.10f, 0.07f, 0.05f }, false },
            { { 0.01f, 0.01f, 0.01f, 0.01f, 0.03f, 0.05f }, false },
            { { 0.01f, 0.01f, 0.01f, 0.01f, 0.02f, 0.03f }, false },
            { { 0.01f, 0.01f, 0.01f, 0.01f, 0.01f, 0.01f }, false },
            { { 0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.00f }, false },
            // Organic (13-21)
            { { 0.50f, 0.50f, 0.50f, 0.50f, 0.50f, 0.50f }, true }, // 13: Vocal Tract
            { { 0.10f, 0.20f, 0.30f, 0.40f, 0.45f, 0.50f }, false },
            { { 0.05f, 0.04f, 0.03f, 0.03f, 0.02f, 0.02f }, false },
            { { 0.15f, 0.25f, 0.35f, 0.40f, 0.55f, 0.70f }, false },
            { { 0.15f, 0.40f, 0.65f, 0.75f, 0.85f, 0.90f }, false },
            { { 0.20f, 0.50f, 0.80f, 0.90f, 0.75f, 0.60f }, false },
            { { 0.02f, 0.02f, 0.03f, 0.03f, 0.04f, 0.05f }, false },
            { { 0.15f, 0.20f, 0.25f, 0.30f, 0.45f, 0.60f }, true }, // 20: Muscle
            { { 0.05f, 0.07f, 0.09f, 0.10f, 0.15f, 0.20f }, false },
            // Transmissive (22-25)
            { { 0.05f, 0.10f, 0.20f, 0.30f, 0.20f, 0.10f }, false },
            { { 0.40f, 0.50f, 0.10f, 0.05f, 0.05f, 0.05f }, false },
            { { 0.15f, 0.25f, 0.35f, 0.45f, 0.55f, 0.65f }, false },
            { { 0.10f, 0.30f, 0.50f, 0.70f, 0.85f, 0.95f }, false },
            // Nature (26-29)
            { { 0.01f, 0.01f, 0.01f, 0.02f, 0.02f, 0.10f }, false },
            { { 0.05f, 0.20f, 0.50f, 0.80f, 0.95f, 0.99f }, false },
            { { 0.60f, 0.70f, 0.80f, 0.85f, 0.90f, 0.95f }, false },
            { { 0.30f, 0.40f, 0.50f, 0.50f, 0.40f, 0.30f }, true }, // 29: Swamp
            // Sci-Fi (30-33)
            { { 0.10f, 0.30f, 0.80f, 0.90f, 0.95f, 0.99f }, false },
            { { 0.10f, 0.15f, 0.10f, 0.15f, 0.10f, 0.15f }, true }, // 31: Plasma
            { { 0.0001f, 0.0001f, 0.0001f, 0.0001f, 0.0001f, 0.0001f }, false },
            { { 0.99f, 0.99f, 0.01f, 0.01f, 0.99f, 0.99f }, true } // 33: Force Field
        };
        if (index < 0 || index >= 34) return data[0];
        return data[index];
    }
}

// ==============================================================================
// 3. DSP MODULES
// ==============================================================================

class DynamicsProcessor {
    float envelope = 0.0f;
    float attackCoef = 0.0f;
    float releaseCoef = 0.0f;
    float thresholdDB = -20.0f;
    float ratio = 2.0f;
    float sampleRate = 48000.0f;

public:
    void prepare(float fs) {
        sampleRate = fs;
        setParams(-20.0f, 2.0f, 10.0f, 100.0f);
        reset();
    }
    void reset() { envelope = 0.0f; }
    void setParams(float thresh, float r, float attMs, float relMs) {
        thresholdDB = thresh;
        ratio = std::max(1.01f, r);
        attackCoef = std::exp(-1000.0f / (std::max(1.0f, attMs) * sampleRate));
        releaseCoef = std::exp(-1000.0f / (std::max(1.0f, relMs) * sampleRate));
    }
    inline float process(float input, float amount) {
        if (std::abs(amount) < 0.01f) return 1.0f;
        float absIn = std::abs(input);
        if (absIn > envelope) envelope = attackCoef * envelope + (1.0f - attackCoef) * absIn;
        else envelope = releaseCoef * envelope + (1.0f - releaseCoef) * absIn;
        envelope = antiDenormal(envelope);
        float envDB = 20.0f * std::log10(std::max(1.0e-6f, envelope));
        float gainChangeDB = 0.0f;
        if (amount < 0.0f) {
            if (envDB > thresholdDB) {
                float over = envDB - thresholdDB;
                gainChangeDB = -over * (1.0f - 1.0f / ratio);
            }
        }
        else {
            if (envDB > thresholdDB) {
                float over = envDB - thresholdDB;
                gainChangeDB = over * (1.0f - 1.0f / ratio);
            }
        }
        gainChangeDB *= std::abs(amount);
        if (gainChangeDB > 24.0f) gainChangeDB = 24.0f;
        if (gainChangeDB < -60.0f) gainChangeDB = -60.0f;
        return std::pow(10.0f, gainChangeDB / 20.0f);
    }
};

class TiltEqualizer {
    float b0 = 1.0f, b1 = 0.0f, b2 = 0.0f, a1 = 0.0f, a2 = 0.0f;
    float x1 = 0.0f, x2 = 0.0f, y1 = 0.0f, y2 = 0.0f;
    float sampleRate = 48000.0f;
    float currentGainDB = 0.0f;
public:
    void prepare(float fs) { sampleRate = fs; reset(); }
    void reset() { x1 = x2 = y1 = y2 = 0.0f; b0 = 1.0f; b1 = b2 = a1 = a2 = 0.0f; currentGainDB = 0.0f; }
    void setTilt(float gainDB) {
        if (std::abs(gainDB - currentGainDB) < 0.01f) return;
        currentGainDB = gainDB;
        float gain = std::pow(10.0f, gainDB / 20.0f);
        float freq = 1000.0f;
        float w0 = 2.0f * PI * freq / sampleRate;
        float A = std::sqrt(gain);
        float cosW = std::cos(w0);
        float sinW = std::sin(w0);
        float alpha = sinW / 2.0f * std::sqrt((A + 1.0f / A) * (1.0f / 0.707f - 1.0f) + 2.0f);
        float a0_inv = 1.0f / ((A + 1.0f) - (A - 1.0f) * cosW + 2.0f * std::sqrt(A) * alpha);
        float pivotGain = 1.0f / A;
        b0 = A * ((A + 1.0f) + (A - 1.0f) * cosW + 2.0f * std::sqrt(A) * alpha) * a0_inv * pivotGain;
        b1 = -2.0f * A * ((A - 1.0f) + (A + 1.0f) * cosW) * a0_inv * pivotGain;
        b2 = A * ((A + 1.0f) + (A - 1.0f) * cosW - 2.0f * std::sqrt(A) * alpha) * a0_inv * pivotGain;
        a1 = 2.0f * ((A - 1.0f) - (A + 1.0f) * cosW) * a0_inv;
        a2 = ((A + 1.0f) - (A - 1.0f) * cosW - 2.0f * std::sqrt(A) * alpha) * a0_inv;
    }
    inline float process(float in) {
        float out = b0 * in + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2;
        out = antiDenormal(out);
        x2 = x1; x1 = in; y2 = y1; y1 = out;
        return out;
    }
};

class DCBlocker {
    float x1 = 0.0f, y1 = 0.0f;
    static constexpr float R = 0.9975f;
public:
    void reset() { x1 = 0.0f; y1 = 0.0f; }
    inline float process(float x) {
        float y = x - x1 + R * y1;
        y = antiDenormal(y);
        x1 = x; y1 = y;
        return y1;
    }
};

class ParameterSmoother {
    float currentValue = 0.0f, targetValue = 0.0f, step = 0.0f;
    int countdown = 0;
public:
    void setTarget(float target, int samplesToSmooth) {
        targetValue = target;
        if (samplesToSmooth <= 0) { currentValue = target; countdown = 0; }
        else { step = (target - currentValue) / (float)samplesToSmooth; countdown = samplesToSmooth; }
    }
    inline float getNext() {
        if (countdown > 0) { currentValue += step; countdown--; }
        else { currentValue = targetValue; }
        return currentValue;
    }
    void snapTo(float value) { currentValue = targetValue = value; countdown = 0; step = 0.0f; }
    float getCurrent() const { return currentValue; }
};

class ChaosLFO {
    float phase1 = 0.0f, phase2 = 0.0f, inc1 = 0.0f, inc2 = 0.0f;
public:
    void setFrequency(float freq, float sampleRate) {
        float safeFreq = (freq > 0.0f) ? freq : 0.0f;
        inc1 = safeFreq / sampleRate;
        inc2 = (safeFreq * 1.41421356f) / sampleRate;
    }
    void setPhase(float p) { phase1 = p; phase2 = p * 1.618f; }
    inline float process() {
        if (inc1 <= 0.0f) return 0.0f;
        phase1 += inc1; if (phase1 >= 1.0f) phase1 -= 1.0f;
        phase2 += inc2; if (phase2 >= 1.0f) phase2 -= 1.0f;
        return (std::sin(phase1 * PI_2) + std::sin(phase2 * PI_2)) * 0.5f;
    }
};

class LagrangeInterpolator {
public:
    static inline float process(const std::vector<float>& buffer, float readPos) {
        int size = (int)buffer.size();
        if (size == 0) return 0.0f;
        while (readPos < 0.0f) readPos += (float)size;
        while (readPos >= (float)size) readPos -= (float)size;
        int i0 = (int)readPos;
        float frac = readPos - (float)i0;
        int im1 = (i0 - 1 + size) % size;
        int i1 = (i0 + 1) % size;
        int i2 = (i0 + 2) % size;
        float ym1 = buffer[im1], y0 = buffer[i0], y1 = buffer[i1], y2 = buffer[i2];
        float c0 = y0;
        float c1 = y1 - ym1 * (1.0f / 3.0f) - y0 * 0.5f - y2 * (1.0f / 6.0f);
        float c2 = (ym1 + y1) * 0.5f - y0;
        float c3 = (y0 - y1) * (1.0f / 2.0f) + (y2 - ym1) * (1.0f / 6.0f);
        return ((c3 * frac + c2) * frac + c1) * frac + c0;
    }
};

class LoopAllpass {
    std::vector<float> buffer;
    int writePos = 0;
    int currentDelayLen = 0;
    float gain = 0.0f;
    float lfoPhase = 0.0f;
    float lfoInc = 0.0f;
    float lfoDepth = 0.0f;
public:
    void setup(int maxLen, float sampleRate) {
        buffer.resize(maxLen + 128, 0.0f);
        currentDelayLen = findNearestPrime(std::max(1, maxLen / 2));
    }
    void setBaseDelay(int samples) {
        currentDelayLen = findNearestPrime(std::max(1, samples));
    }
    void setGain(float g) { gain = g; }
    void setModulation(float rate, float depth, float sampleRate, int channelIdx) {
        if (depth <= 0.001f) { lfoInc = 0.0f; lfoDepth = 0.0f; return; }
        lfoInc = ((0.05f + rate * 0.55f) * PI_2) / sampleRate;
        lfoDepth = depth * std::min(12.0f, (float)currentDelayLen * 0.15f);
        lfoPhase = (float)channelIdx * 0.618f * PI_2;
    }
    void reset() { std::fill(buffer.begin(), buffer.end(), 0.0f); writePos = 0; lfoPhase = 0.0f; }
    inline float process(float in) {
        float mod = 0.0f;
        if (lfoDepth > 0.0f) {
            lfoPhase += lfoInc;
            if (lfoPhase > PI_2) lfoPhase -= PI_2;
            mod = std::sin(lfoPhase) * lfoDepth;
        }
        float readPos = (float)writePos - (float)currentDelayLen + mod;
        int bufSize = (int)buffer.size();
        while (readPos < 0.0f) readPos += (float)bufSize;
        while (readPos >= (float)bufSize) readPos -= (float)bufSize;
        int idxA = (int)readPos;
        float frac = readPos - (float)idxA;
        float delayed = buffer[idxA] * (1.0f - frac) + buffer[(idxA + 1) % bufSize] * frac;
        float v_n = in + gain * delayed;
        v_n = safeLoopSaturate(v_n);
        v_n = antiDenormal(v_n);
        buffer[writePos] = v_n;
        float out = delayed - gain * v_n;
        writePos = (writePos + 1) % bufSize;
        return out;
    }
};

class OnePoleHighpass {
    float x1 = 0.0f, y1 = 0.0f, a1 = 0.0f, b0 = 1.0f, b1 = -1.0f;
public:
    void reset() { x1 = 0.0f; y1 = 0.0f; }
    void setFrequency(float freq, float sampleRate) {
        float w = 2.0f * PI * freq / sampleRate;
        if (w >= PI) w = PI - 0.001f;
        float p = std::exp(-w);
        b0 = (1.0f + p) * 0.5f; b1 = -(1.0f + p) * 0.5f; a1 = -p;
    }
    inline float process(float in) {
        float out = b0 * in + b1 * x1 - a1 * y1;
        out = antiDenormal(out);
        x1 = in; y1 = out;
        return out;
    }
};

class HighQualityFilter {
    struct BiquadSection {
        float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
        float x1 = 0, x2 = 0, y1 = 0, y2 = 0;
        void reset() { x1 = x2 = y1 = y2 = 0.0f; }
        inline float process(float in) {
            float out = b0 * in + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2;
            out = antiDenormal(out);
            x2 = x1; x1 = in; y2 = y1; y1 = out;
            return y1;
        }
    };
    BiquadSection lpSection1, lpSection2;
    BiquadSection hpSection1, hpSection2;
    float currentLPFreq = 20000.0f;
    float currentHPFreq = 20.0f;
    float sampleRate = 48000.0f;
public:
    void prepare(float fs) { sampleRate = fs; reset(); }
    void reset() { lpSection1.reset(); lpSection2.reset(); hpSection1.reset(); hpSection2.reset(); }
    void setLowPass(float freq) {
        freq = std::clamp(freq, 20.0f, sampleRate * 0.49f);
        currentLPFreq = freq;
        calculateLPCoeffs(lpSection1, freq, 0.707f);
        calculateLPCoeffs(lpSection2, freq, 0.707f);
    }
    void setHighPass(float freq) {
        freq = std::clamp(freq, 20.0f, sampleRate * 0.49f);
        currentHPFreq = freq;
        calculateHPCoeffs(hpSection1, freq, 0.707f);
        calculateHPCoeffs(hpSection2, freq, 0.707f);
    }
    inline float process(float in) {
        return lpSection2.process(lpSection1.process(hpSection2.process(hpSection1.process(in))));
    }
private:
    void calculateLPCoeffs(BiquadSection& s, float freq, float Q) {
        float w0 = 2.0f * PI * freq / sampleRate;
        float alpha = std::sin(w0) / (2.0f * Q);
        float cosw0 = std::cos(w0);
        float a0 = 1.0f + alpha;
        float invA0 = 1.0f / a0;
        s.b0 = ((1.0f - cosw0) / 2.0f) * invA0;
        s.b1 = (1.0f - cosw0) * invA0;
        s.b2 = ((1.0f - cosw0) / 2.0f) * invA0;
        s.a1 = (-2.0f * cosw0) * invA0;
        s.a2 = (1.0f - alpha) * invA0;
    }
    void calculateHPCoeffs(BiquadSection& s, float freq, float Q) {
        float w0 = 2.0f * PI * freq / sampleRate;
        float alpha = std::sin(w0) / (2.0f * Q);
        float cosw0 = std::cos(w0);
        float a0 = 1.0f + alpha;
        float invA0 = 1.0f / a0;
        s.b0 = ((1.0f + cosw0) / 2.0f) * invA0;
        s.b1 = (-(1.0f + cosw0)) * invA0;
        s.b2 = ((1.0f + cosw0) / 2.0f) * invA0;
        s.a1 = (-2.0f * cosw0) * invA0;
        s.a2 = (1.0f - alpha) * invA0;
    }
};

class MaterialFilter4Band {
    struct SmoothedBiquad {
        float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
        float tb0 = 1, tb1 = 0, tb2 = 0, ta1 = 0, ta2 = 0;
        float x1 = 0, x2 = 0, y1 = 0, y2 = 0;
        int rampCounter = 0;
        void reset() { x1 = x2 = y1 = y2 = 0.0f; b0 = tb0 = 1; b1 = tb1 = 0; b2 = tb2 = 0; a1 = ta1 = 0; a2 = ta2 = 0; }
        void setTarget(float n_b0, float n_b1, float n_b2, float n_a1, float n_a2) {
            tb0 = n_b0; tb1 = n_b1; tb2 = n_b2; ta1 = n_a1; ta2 = n_a2;
            rampCounter = 64;
        }
        inline float process(float in) {
            if (rampCounter > 0) {
                float alpha = 1.0f / (float)rampCounter;
                b0 += (tb0 - b0) * alpha;
                b1 += (tb1 - b1) * alpha;
                b2 += (tb2 - b2) * alpha;
                a1 += (ta1 - a1) * alpha;
                a2 += (ta2 - a2) * alpha;
                rampCounter--;
            }
            else {
                b0 = tb0; b1 = tb1; b2 = tb2; a1 = ta1; a2 = ta2;
            }
            float out = b0 * in + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2;
            out = antiDenormal(out);
            x2 = x1; x1 = in; y2 = y1; y1 = out;
            return out;
        }
    };
    SmoothedBiquad ls, p1, p2, hs;
    ParameterSmoother midGainSmoother;

public:
    void reset() {
        ls.reset(); p1.reset(); p2.reset(); hs.reset();
        midGainSmoother.snapTo(1.0f);
    }
    void setCoeffs(const std::array<float, 6>& targetGains, float baseG, float fs) {
        midGainSmoother.setTarget(baseG, 64);
        float safeBase = (baseG > 0.000001f) ? baseG : 0.000001f;
        auto calcLS = [&](float g) {
            float A = std::sqrt(g);
            float w0 = 2.0f * PI * 200.0f / fs;
            float alpha = std::sin(w0) / 2.0f * std::sqrt((A + 1 / A) * (1 / 0.707f - 1) + 2);
            float cosw0 = std::cos(w0);
            float a0 = (A + 1) + (A - 1) * cosw0 + 2 * std::sqrt(A) * alpha;
            float a0_inv = 1.0f / a0;
            ls.setTarget(
                A * ((A + 1) - (A - 1) * cosw0 + 2 * std::sqrt(A) * alpha) * a0_inv,
                2 * A * ((A - 1) - (A + 1) * cosw0) * a0_inv,
                A * ((A + 1) - (A - 1) * cosw0 - 2 * std::sqrt(A) * alpha) * a0_inv,
                -2 * ((A - 1) + (A + 1) * cosw0) * a0_inv,
                ((A + 1) + (A - 1) * cosw0 - 2 * std::sqrt(A) * alpha) * a0_inv
            );
            };
        auto calcPeak = [&](float g, float freq, float Q, SmoothedBiquad& bq) {
            float A = std::sqrt(g);
            float w0 = 2.0f * PI * freq / fs;
            float alpha = std::sin(w0) / (2.0f * Q);
            float cosw0 = std::cos(w0);
            float a0_inv = 1.0f / (1.0f + alpha / A);
            bq.setTarget(
                (1.0f + alpha * A) * a0_inv,
                (-2.0f * cosw0) * a0_inv,
                (1.0f - alpha * A) * a0_inv,
                (-2.0f * cosw0) * a0_inv,
                (1.0f - alpha / A) * a0_inv
            );
            };
        auto calcHS = [&](float g) {
            float A = std::sqrt(g);
            float w0 = 2.0f * PI * 3000.0f / fs;
            float alpha = std::sin(w0) / 2.0f * std::sqrt((A + 1 / A) * (1 / 0.707f - 1) + 2);
            float cosw0 = std::cos(w0);
            float a0 = (A + 1) - (A - 1) * cosw0 + 2 * std::sqrt(A) * alpha;
            float a0_inv = 1.0f / a0;
            hs.setTarget(
                A * ((A + 1) + (A - 1) * cosw0 + 2 * std::sqrt(A) * alpha) * a0_inv,
                -2 * A * ((A - 1) + (A + 1) * cosw0) * a0_inv,
                A * ((A + 1) + (A - 1) * cosw0 - 2 * std::sqrt(A) * alpha) * a0_inv,
                2 * ((A - 1) - (A + 1) * cosw0) * a0_inv,
                ((A + 1) - (A - 1) * cosw0 - 2 * std::sqrt(A) * alpha) * a0_inv
            );
            };
        calcLS(std::clamp(targetGains[0] / safeBase, 0.0f, 1.0f));
        calcPeak(std::clamp(targetGains[2] / safeBase, 0.0f, 1.0f), 500.0f, 0.5f, p1);
        calcPeak(std::clamp(targetGains[4] / safeBase, 0.0f, 1.0f), 2000.0f, 0.5f, p2);
        calcHS(std::clamp(targetGains[5] / safeBase, 0.0f, 1.0f));
    }
    inline float process(float in) {
        float x = in * midGainSmoother.getNext();
        x = ls.process(x); x = p1.process(x); x = p2.process(x); x = hs.process(x);
        return x;
    }
};

struct VelvetNoiseDiffuser {
    std::vector<float> buffer;
    int writePos = 0;
    int bufferMask = 0;
    struct Tap { int delay; float gain; };
    std::vector<Tap> taps;
    float amount = 0.0f;

    void prepare(float fs) {
        float durationMs = 50.0f;
        int minSamples = (int)(durationMs * fs * 0.001f) + 1024;
        int size = 1;
        while (size < minSamples) size *= 2;
        if ((int)buffer.size() != size) buffer.assign(size, 0.0f);
        else std::fill(buffer.begin(), buffer.end(), 0.0f);
        bufferMask = size - 1;
        writePos = 0;
        taps.clear();
        int numTaps = 48;
        float totalSamples = durationMs * fs * 0.001f;
        float grid = totalSamples / (float)numTaps;
        std::mt19937 gen(12345);
        std::uniform_real_distribution<float> distOffset(0.0f, grid - 1.0f);
        std::uniform_int_distribution<> sign(0, 1);
        float normGain = 1.0f / std::sqrt((float)numTaps);
        for (int i = 0; i < numTaps; ++i) {
            Tap t;
            float gridStart = (float)i * grid;
            float offset = distOffset(gen);
            t.delay = (int)(gridStart + offset);
            if (t.delay < 1) t.delay = 1;
            if (t.delay >= size) t.delay = size - 1;
            t.gain = (sign(gen) == 0 ? 1.0f : -1.0f) * normGain;
            taps.push_back(t);
        }
    }
    void setAmount(float a) { amount = a; }
    void reset() { std::fill(buffer.begin(), buffer.end(), 0.0f); writePos = 0; }
    inline float process(float in) {
        if (amount < 0.01f) return in;
        if (buffer.empty()) return in;
        buffer[writePos] = in;
        float out = 0.0f;
        for (const auto& t : taps) {
            int r = (writePos - t.delay) & bufferMask;
            out += buffer[r] * t.gain;
        }
        writePos = (writePos + 1) & bufferMask;
        return in * (1.0f - amount) + out * amount;
    }
};

struct EarlyReflections {
    std::vector<float> predelayBuffer;
    int preWritePos = 0;
    float fs = 48000.0f;
    struct Reflection {
        int delaySamples = 0;
        float gain = 0.0f;
        float panL = 0.5f;
        float panR = 0.5f;
    };
    std::array<Reflection, 7> taps;
    EarlyReflections() {}
    void prepare(double sampleRate) {
        fs = (float)sampleRate;
        int size = (int)(static_cast<double>(MAX_DELAY_SECONDS) * sampleRate) + 4096;
        if (predelayBuffer.size() < size) predelayBuffer.resize(size, 0.0f);
        reset();
    }
    void reset() { std::fill(predelayBuffer.begin(), predelayBuffer.end(), 0.0f); preWritePos = 0; }
    void updateGeometry(float W, float D, float H, float predelayMs, float srcH, float diffusion, float dist, float pan) {
        float c = SPEED_OF_SOUND;
        int baseDelay = (int)(predelayMs * 0.001f * fs);
        float sx = pan * (W * 0.45f);
        float sy = (srcH - 0.5f) * H;
        float sz = (dist - 0.5f) * D;
        float lx = 0.0f;
        float ly = 0.0f;
        float lz = -D * 0.45f;
        float dDirect = std::sqrt(std::pow(sx - lx, 2) + std::pow(sy - ly, 2) + std::pow(sz - lz, 2));
        float dFloor = std::sqrt(std::pow(sx - lx, 2) + std::pow(-H - sy - ly, 2) + std::pow(sz - lz, 2));
        float dCeil = std::sqrt(std::pow(sx - lx, 2) + std::pow(H + (H / 2.0f - sy) - ly, 2) + std::pow(sz - lz, 2));
        float dLeft = std::sqrt(std::pow(-W - sx - lx, 2) + std::pow(sy - ly, 2) + std::pow(sz - lz, 2));
        float dRight = std::sqrt(std::pow(W + (W / 2.0f - sx) - lx, 2) + std::pow(sy - ly, 2) + std::pow(sz - lz, 2));
        float dFront = std::sqrt(std::pow(sx - lx, 2) + std::pow(sy - ly, 2) + std::pow(D + (D / 2.0f - sz) - lz, 2));
        float dBack = std::sqrt(std::pow(sx - lx, 2) + std::pow(sy - ly, 2) + std::pow(-D - sz - lz, 2));
        float paths[7] = { dFloor, dCeil, dLeft, dRight, dFront, dBack, (dFloor + dCeil + dLeft + dRight) * 0.25f + 1.0f };
        int maxBuf = (int)predelayBuffer.size();
        for (int i = 0; i < 7; ++i) {
            float pathDiff = paths[i] - dDirect;
            if (pathDiff < 0.1f) pathDiff = 0.1f;
            float timeSec = pathDiff / c;
            int samp = baseDelay + (int)(timeSec * fs);
            taps[i].delaySamples = std::clamp(samp, 1, maxBuf - 1);
            float distAtten = 1.0f / (1.0f + paths[i] * 0.5f);
            taps[i].gain = distAtten * (1.0f - diffusion * 0.3f);
            if (i == 2) { taps[i].panL = 0.9f; taps[i].panR = 0.1f; }
            else if (i == 3) { taps[i].panL = 0.1f; taps[i].panR = 0.9f; }
            else { taps[i].panL = 0.5f; taps[i].panR = 0.5f; }
        }
    }
    inline void process(float input, float& outL, float& outR) {
        int size = (int)predelayBuffer.size();
        predelayBuffer[preWritePos] = input;
        float sumL = 0.0f;
        float sumR = 0.0f;
        for (const auto& tap : taps) {
            int idx = preWritePos - tap.delaySamples;
            if (idx < 0) idx += size;
            float val = predelayBuffer[idx] * tap.gain;
            sumL += val * tap.panL;
            sumR += val * tap.panR;
        }
        outL = sumL;
        outR = sumR;
        preWritePos++;
        if (preWritePos >= size) preWritePos = 0;
    }
};

struct alignas(64) FDNChannel {
    std::vector<float> buffer;
    int writePos = 0;
    ChaosLFO lfo;
    MaterialFilter4Band materialFilter;
    LoopAllpass loopAllpass1;
    LoopAllpass loopAllpass2;
    DCBlocker feedbackDCBlocker;
    ParameterSmoother delaySmoother;
    ParameterSmoother gainSmoother;
    ParameterSmoother densitySmoother;
    FDNChannel() {}
    void prepare(double sampleRate) {
        int size = (int)(MAX_DELAY_SECONDS * sampleRate) + 4096;
        if (buffer.size() < size) {
            buffer.resize(size, 0.0f);
        }
        loopAllpass1.setup(4096, (float)sampleRate);
        loopAllpass2.setup(4096, (float)sampleRate);
    }
    inline void push(float sample) {
        sample = feedbackDCBlocker.process(sample);
        sample = antiDenormal(sample);
        buffer[writePos] = sample;
        writePos++;
        if (writePos >= (int)buffer.size()) writePos = 0;
    }
    inline float read(float modDepth) {
        float lfoVal = 0.0f;
        if (modDepth > 0.001f) lfoVal = lfo.process();
        float currentDelay = delaySmoother.getNext();
        float modulatedDelay = currentDelay + (lfoVal * modDepth);
        if (modulatedDelay < 2.0f) modulatedDelay = 2.0f;
        float r = (float)writePos - modulatedDelay;
        int size = (int)buffer.size();
        if (r < 0.0f) r += (float)size;
        else if (r >= (float)size) r -= (float)size;
        return LagrangeInterpolator::process(buffer, r);
    }
    inline float getSmoothedGain() { return gainSmoother.getNext(); }
    void setDensity(float amount, int samplesToSmooth) {
        float g = amount * 0.6f;
        densitySmoother.setTarget(g, samplesToSmooth);
    }
    inline float processFilter(float sample) {
        float g = densitySmoother.getNext();
        loopAllpass1.setGain(g);
        loopAllpass2.setGain(g);
        float out = materialFilter.process(sample);
        out = loopAllpass1.process(out);
        out = loopAllpass2.process(out);
        return out;
    }
    void reset() {
        std::fill(buffer.begin(), buffer.end(), 0.0f);
        writePos = 0;
        materialFilter.reset();
        loopAllpass1.reset();
        loopAllpass2.reset();
        feedbackDCBlocker.reset();
        delaySmoother.snapTo(1000.0f);
        gainSmoother.snapTo(0.0f);
        densitySmoother.snapTo(0.0f);
    }
};

// --- MATRIX FUNCTIONS ---
static inline void matrixHadamard(float* x) {
    for (int i = 0; i < 16; i += 2) { float a = x[i]; float b = x[i + 1]; x[i] = a + b; x[i + 1] = a - b; }
    for (int i = 0; i < 16; i += 4) {
        float a0 = x[i]; float b0 = x[i + 2]; x[i] = a0 + b0; x[i + 2] = a0 - b0;
        float a1 = x[i + 1]; float b1 = x[i + 3]; x[i + 1] = a1 + b1; x[i + 3] = a1 - b1;
    }
    for (int i = 0; i < 16; i += 8) {
        for (int j = 0; j < 4; ++j) {
            float a = x[i + j]; float b = x[i + j + 4]; x[i + j] = a + b; x[i + j + 4] = a - b;
        }
    }
    for (int i = 0; i < 8; ++i) { float a = x[i]; float b = x[i + 8]; x[i] = a + b; x[i + 8] = a - b; }
    for (int i = 0; i < 16; ++i) x[i] *= 0.25f;
}
static inline void matrixHouseholder(float* x) {
    float sum = 0.0f;
    for (int i = 0; i < 16; ++i) sum += x[i];
    float u = -2.0f / 16.0f;
    float s = sum * u;
    for (int i = 0; i < 16; ++i) x[i] += s;
}
static inline void matrixBlockPerm(float* x) {
    auto mix4 = [](float* p) {
        float a = p[0], b = p[1], c = p[2], d = p[3];
        p[0] = 0.5f * (a + b + c + d); p[1] = 0.5f * (a - b + c - d); p[2] = 0.5f * (a + b - c - d); p[3] = 0.5f * (a - b - c + d);
        };
    mix4(&x[0]); mix4(&x[4]); mix4(&x[8]); mix4(&x[12]);
    float temp = x[1]; x[1] = x[5]; x[5] = x[9]; x[9] = x[13]; x[13] = temp;
    temp = x[2]; x[2] = x[6]; x[6] = x[10]; x[10] = x[14]; x[14] = temp;
    temp = x[3]; x[3] = x[7]; x[7] = x[11]; x[11] = x[15]; x[15] = temp;
}
static inline void matrixCylinder(float* x) {
    for (int i = 0; i < 16; i += 2) {
        float a = x[i]; float b = x[i + 1];
        x[i] = 0.707f * (a - b);
        x[i + 1] = 0.707f * (a + b);
    }
    float temp = x[0];
    for (int i = 0; i < 15; ++i) x[i] = x[i + 1];
    x[15] = temp;
}
static inline void matrixSparse(float* x) {
    for (int i = 0; i < 16; i += 2) {
        float a = x[i]; float b = x[i + 1];
        x[i] = 0.707f * (a - b); x[i + 1] = 0.707f * (a + b);
    }
    std::swap(x[1], x[4]); std::swap(x[5], x[8]); std::swap(x[9], x[12]);
}
static inline void matrixMDS(float* x) {
    matrixHadamard(x);
    for (int i = 1; i < 16; i += 2) x[i] = -x[i];
    matrixHadamard(x);
}
static inline void matrixChaos(float* x) {
    float temp[16] = { 0.0f };
    static const int p[16] = { 3, 15, 4, 0, 8, 12, 1, 5, 9, 2, 6, 10, 14, 7, 11, 13 };
    for (int i = 0; i < 16; ++i) temp[i] = x[p[i]];
    matrixHadamard(temp);
    for (int i = 0; i < 16; ++i) x[i] = temp[i];
}

struct RT60Data {
    std::array<float, 6> decay = { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f };
};

class FDNEngine {
public:
    FDNEngine() {
        stereoSpreadBuffer.resize(2048, 0.0f);
        reset();
    }

    void prepare(double sampleRate) {
        fs = sampleRate;
        int inDelaySize = (int)(static_cast<double>(MAX_DELAY_SECONDS) * sampleRate) + 4096;
        if (inputDelayBuffer.size() < inDelaySize) {
            inputDelayBuffer.resize(inDelaySize, 0.0f);
        }
        stereoSpreadSamples = std::max(1, (int)(STEREO_SPREAD_MS * 0.001f * sampleRate));
        if (stereoSpreadSamples > 2048) stereoSpreadSamples = 2048;
        for (int i = 0; i < FDN_CHANNELS; ++i) {
            channels[i].lfo.setFrequency(0.5f * LFO_RATIOS[i], (float)fs);
            channels[i].lfo.setPhase((float)i / (float)FDN_CHANNELS);
            channels[i].prepare(sampleRate);
        }
        inFilterL.prepare((float)fs); inFilterR.prepare((float)fs);
        outFilterL.prepare((float)fs); outFilterR.prepare((float)fs);
        velvetL.prepare((float)fs); velvetR.prepare((float)fs);
        sideHPF.setFrequency(200.0f, (float)fs);
        sideHPF.reset();
        erEngine.prepare(sampleRate);
        dynamicsProcessor.prepare((float)sampleRate);
        tiltEQ_L.prepare((float)sampleRate);
        tiltEQ_R.prepare((float)sampleRate);
        reset();
    }

    void reset() {
        for (int i = 0; i < FDN_CHANNELS; ++i) {
            channels[i].reset();
            channels[i].lfo.setPhase((float)i / (float)FDN_CHANNELS);
        }
        std::fill(inputDelayBuffer.begin(), inputDelayBuffer.end(), 0.0f);
        inputDelayWritePos = 0;
        std::fill(stereoSpreadBuffer.begin(), stereoSpreadBuffer.end(), 0.0f);
        stereoSpreadWritePos = 0;
        dryGainSmoother.snapTo(1.0f);
        wetGainSmoother.snapTo(0.0f);
        erEngine.reset();
        dcBlockerL.reset(); dcBlockerR.reset();
        inFilterL.reset(); inFilterR.reset();
        outFilterL.reset(); outFilterR.reset();
        velvetL.reset(); velvetR.reset();
        sideHPF.reset();
        dynamicsProcessor.reset();
        tiltEQ_L.reset(); tiltEQ_R.reset();
    }

    void updatePhysics(float widthM, float depthM, float heightM,
        int matFloorIdx, int matCeilIdx, int matWallIdx, int matWallFBIdx,
        float absorptionOverride,
        float modRate, float modDepth, float predelayMs,
        float tempC, float humidityPct, float dryWet,
        float inLC, float inHC, float outLC, float outHC,
        float sourceDist, float sourcePan, float sourceHeight,
        int roomShape, float diffusion, float stereoWidth, float outputLevel,
        float density, float drive, float dynamicsAmount, float tilt,
        float dynThreshold, float dynRatio, float dynAttack, float dynRelease,
        int samplesPerBlock, float decayRatio) {

        currentShapeMode = roomShape;

        // Check for SFX Materials (Priority: Floor > Ceil > Wall)
        int sfxType = 0; // 0: Normal
        auto checkSFX = [&](int mat) {
            if (mat == 13) return 1; // Vocal Tract
            if (mat == 20) return 2; // Muscle
            if (mat == 29) return 3; // Swamp
            if (mat == 31) return 4; // Plasma
            if (mat == 33) return 5; // Force Field
            return 0;
            };

        sfxType = checkSFX(matFloorIdx);
        if (sfxType == 0) sfxType = checkSFX(matCeilIdx);
        if (sfxType == 0) sfxType = checkSFX(matWallIdx);
        if (sfxType == 0) sfxType = checkSFX(matWallFBIdx);

        float volFactor = 1.0f;
        if (roomShape == 1) volFactor = 0.65f;
        else if (roomShape == 2) volFactor = 0.75f;
        else if (roomShape == 3) volFactor = 0.785f;
        else if (roomShape == 4) volFactor = 0.333f;

        float areaFloor = widthM * depthM;
        float areaCeil = widthM * depthM;
        float areaSide = 2.0f * (depthM * heightM);
        float areaFB = 2.0f * (widthM * heightM);
        float totalArea = areaFloor + areaCeil + areaSide + areaFB;

        if (roomShape == 1) totalArea *= 0.8f;
        if (roomShape == 4) totalArea *= 0.6f;

        float volume = widthM * depthM * heightM * volFactor;
        float mfp = (4.0f * volume / std::max(1.0f, totalArea));
        float baseDelaySec = mfp / SPEED_OF_SOUND;

        std::array<float, 16> ratios = { 0.0f };
        static const float silkyRatios[16] = {
            1.0000f, 1.0931f, 1.1953f, 1.3072f, 1.4298f, 1.5641f, 1.7112f, 1.8723f,
            2.0489f, 2.2421f, 2.4532f, 2.6843f, 2.9371f, 3.2134f, 3.5156f, 3.8462f
        };
        for (int i = 0; i < 16; ++i) ratios[i] = silkyRatios[i];

        if (roomShape == 3) { for (int i = 0; i < 16; ++i) ratios[i] = 1.0f + (float)i * 0.1f; }
        else if (roomShape == 5) { for (int i = 0; i < 16; ++i) ratios[i] = 1.0f + ((i % 4) * 0.05f); }
        else if (roomShape == 6) {
            static const float chaosRatios[16] = { 0.3f, 1.9f, 0.7f, 2.3f, 1.1f, 0.5f, 2.9f, 1.3f, 0.9f, 2.1f, 0.4f, 1.7f, 2.5f, 0.8f, 1.5f, 0.6f };
            for (int i = 0; i < 16; ++i) ratios[i] = chaosRatios[i];
        }

        // Restore ratioSum calculation
        float ratioSum = 0.0f;
        for (float r : ratios) ratioSum += r;

        float targetDelays[16] = { 0.0f };
        int maxChBuf = (int)channels[0].buffer.size();
        for (int i = 0; i < FDN_CHANNELS; ++i) {
            float rawDelay = baseDelaySec * ratios[i] * (float)fs;
            int primeDelay = findNearestPrime((int)rawDelay);
            targetDelays[i] = (float)primeDelay;
            if (targetDelays[i] > maxChBuf - 4096) targetDelays[i] = (float)(maxChBuf - 4096);
        }

        currentDrive = drive;
        dynamicsProcessor.setParams(dynThreshold, dynRatio, dynAttack, dynRelease);
        currentDynamicsAmount = dynamicsAmount;
        tiltEQ_L.setTilt(tilt);
        tiltEQ_R.setTilt(tilt);
        velvetL.setAmount(diffusion);
        velvetR.setAmount(diffusion);

        // SFX Mode Overrides
        if (sfxType > 0) {
            float sfxFeedback = std::clamp(decayRatio, 0.01f, 0.99f);
            std::array<float, 6> sfxGains;
            sfxGains.fill(sfxFeedback);

            lastRT60Data.decay.fill(0.0f);

            for (int i = 0; i < FDN_CHANNELS; ++i) {
                float fixedDelay = 0.0f;
                float lfoFreq = 0.0f;
                float lfoDepthScaled = 0.0f;

                switch (sfxType) {
                case 1: // Vocal Tract
                    fixedDelay = 0.02f + (float)i * 0.0015f;
                    lfoFreq = 2.0f;
                    lfoDepthScaled = 200.0f;
                    break;
                case 2: // Muscle
                    fixedDelay = 0.005f + (float)i * 0.0003f;
                    lfoFreq = 0.0f;
                    sfxGains.fill(sfxFeedback * 0.3f);
                    sfxGains[3] *= 0.5f; sfxGains[4] *= 0.2f; sfxGains[5] *= 0.1f;
                    break;
                case 3: // Swamp
                    fixedDelay = 0.01f + ((i * 7) % 40) * 0.001f;
                    lfoFreq = 6.0f;
                    lfoDepthScaled = 30.0f;
                    break;
                case 4: // Plasma
                    fixedDelay = 0.001f + (float)i * 0.0002f;
                    lfoFreq = 50.0f;
                    lfoDepthScaled = 5.0f;
                    currentDrive = 1.0f;
                    break;
                case 5: // Force Field
                    fixedDelay = 0.015f;
                    lfoFreq = 0.0f;
                    break;
                }

                targetDelays[i] = fixedDelay * (float)fs;
                channels[i].materialFilter.setCoeffs(sfxGains, 1.0f, (float)fs);
                channels[i].lfo.setFrequency(lfoFreq, (float)fs);
                currentModDepth = lfoDepthScaled;
            }
        }
        else
        {
            // --- Normal Physics Mode ---
            MaterialDef mFloor = MaterialDB::get(matFloorIdx);
            MaterialDef mCeil = MaterialDB::get(matCeilIdx);
            MaterialDef mSide = MaterialDB::get(matWallIdx);
            MaterialDef mFB = MaterialDB::get(matWallFBIdx);

            auto weightedAvg = [&](float vF, float vC, float vS, float vFB) {
                return (vF * areaFloor + vC * areaCeil + vS * areaSide + vFB * areaFB) / totalArea;
                };
            std::array<float, 6> avgAbs;
            for (int b = 0; b < 6; ++b) {
                avgAbs[b] = weightedAvg(mFloor.absorption[b], mCeil.absorption[b], mSide.absorption[b], mFB.absorption[b]);
            }
            float scaler = 0.5f + absorptionOverride;
            for (int b = 0; b < 6; ++b) {
                float a = avgAbs[b] * scaler;
                if (a > 0.9f) { float x = (a - 0.9f) / 1.1f; a = 0.9f + (x / (1.0f + x)) * 0.095f; }
                if (a < 0.01f) a = 0.01f;
                avgAbs[b] = a;
            }
            static const float freqs[6] = { 125.0f, 250.0f, 500.0f, 1000.0f, 2000.0f, 4000.0f };
            float exponent = (decayRatio > 0.01f) ? (1.0f / decayRatio) : 100.0f;
            std::array<float, 6> finalGains;
            for (int b = 0; b < 6; ++b) {
                float air = std::pow(calcAirAbsorption(freqs[b], tempC, humidityPct), mfp);
                float r = std::sqrt(1.0f - avgAbs[b]) * air;
                r = std::pow(r, exponent);
                if (r > 0.98f) r = 0.98f;
                finalGains[b] = r;
            }
            float maxG = 0.0001f;
            for (float g : finalGains) { if (g > maxG) maxG = g; }
            float baseGain = (maxG > 0.98f) ? 0.98f : maxG;

            float rateScaled = modRate * 0.4f;
            float depthSkewed = (modDepth < 0.8f) ? (modDepth * 0.25f) : (0.2f + (modDepth - 0.8f) * 4.0f);
            depthSkewed *= (1.0f + diffusion * 0.5f);
            float sampleRateScale = (float)fs / REFERENCE_SAMPLE_RATE;
            currentModDepth = depthSkewed * 20.0f * sampleRateScale;

            for (int i = 0; i < FDN_CHANNELS; ++i) {
                float delaySamples = targetDelays[i];
                if (delaySamples < 1.0f) delaySamples = 1.0f;
                std::array<float, 6> sampleGains;
                for (int b = 0; b < 6; ++b) {
                    float gs = std::pow(finalGains[b], 1.0f / delaySamples);
                    if (gs > 0.9999f) gs = 0.9999f;
                    sampleGains[b] = gs;
                }
                channels[i].materialFilter.setCoeffs(sampleGains, baseGain, (float)fs);
                float uniqueRate = rateScaled * LFO_RATIOS[i];
                channels[i].lfo.setFrequency(uniqueRate, (float)fs);
            }

            // Recalculate RT60 for graph
            lastAvgDelay = baseDelaySec * (ratioSum / 16.0f) * (float)fs;
            for (int b = 0; b < 6; ++b) { lastRT60Data.decay[b] = calcT60(finalGains[b]); }
        }

        // Apply delays & filters
        float apGain = density * 0.15f;
        for (int i = 0; i < FDN_CHANNELS; ++i) {
            channels[i].setDensity(apGain, samplesPerBlock);
            channels[i].loopAllpass1.setModulation(modRate, modDepth, (float)fs, i);
            channels[i].loopAllpass2.setModulation(modRate, modDepth, (float)fs, i + 16);
            int apLen = (int)(targetDelays[i] * 0.3f);
            if (apLen < 8) apLen = 8;
            channels[i].loopAllpass1.setBaseDelay(apLen);
            channels[i].loopAllpass2.setBaseDelay((int)(apLen * 1.5f));
            channels[i].delaySmoother.setTarget(targetDelays[i], samplesPerBlock);
        }

        // Common calculations
        float distanceFactor = std::clamp(sourceDist / depthM, 0.0f, 1.0f);
        float distDelayReduction = distanceFactor * 20.0f;
        float effectivePreDelay = predelayMs - distDelayReduction;
        if (effectivePreDelay < 0.0f) effectivePreDelay = 0.0f;
        currentPreDelaySamples = (int)((effectivePreDelay / 1000.0f) * fs);
        int maxInBuf = (int)inputDelayBuffer.size();
        if (currentPreDelaySamples >= maxInBuf) currentPreDelaySamples = maxInBuf - 1;
        float widthScaling = 1.0f - (distanceFactor * 0.5f);
        currentWidth = stereoWidth * widthScaling;
        float panVal = std::clamp(sourcePan, -1.0f, 1.0f);
        panInputL = 1.0f - std::max(0.0f, panVal);
        panInputR = 1.0f - std::max(0.0f, -panVal);
        float norm = 1.0f / std::sqrt(panInputL * panInputL + panInputR * panInputR);
        panInputL *= norm; panInputR *= norm;
        float safeSrcH = std::clamp(sourceHeight, 0.0f, heightM);
        erEngine.updateGeometry(widthM, depthM, heightM, effectivePreDelay, safeSrcH, diffusion, sourceDist, sourcePan);
        inFilterL.setHighPass(inLC); inFilterL.setLowPass(inHC);
        inFilterR.setHighPass(inLC); inFilterR.setLowPass(inHC);
        outFilterL.setHighPass(outLC); outFilterL.setLowPass(outHC);
        outFilterR.setHighPass(outLC); outFilterR.setLowPass(outHC);

        float baseGainMix = 1.0f / std::max(0.1f, (1.0f - absorptionOverride * 0.5f));
        float sizeComp = 1.0f + std::clamp(volume / 10000.0f, 0.0f, 0.5f);
        float makeup = std::clamp(baseGainMix * sizeComp, 1.0f, 3.0f);
        float wetBoost = 2.4f;
        float mix = std::clamp(dryWet, 0.0f, 1.0f);
        float dryG = std::cos(mix * HALF_PI) * outputLevel;
        float wetG = std::sin(mix * HALF_PI) * outputLevel * makeup * wetBoost;
        int smoothSamples = (int)(0.05f * fs);
        dryGainSmoother.setTarget(dryG, smoothSamples);
        wetGainSmoother.setTarget(wetG, smoothSamples);
    }

    void process(float* const* inputChannelData, float* const* outputChannelData, int numSamples, int numChannels) {
        const float* inL = inputChannelData[0];
        const float* inR = (numChannels > 1) ? inputChannelData[1] : inputChannelData[0];
        float* outL = outputChannelData[0];
        float* outR = (numChannels > 1) ? outputChannelData[1] : outputChannelData[0];

        int inDelaySize = (int)inputDelayBuffer.size();

        for (int n = 0; n < numSamples; ++n) {
            float dryL = inL[n];
            float dryR = inR[n];

            float inputMax = std::max(std::abs(dryL), std::abs(dryR));
            float dynGain = dynamicsProcessor.process(inputMax, currentDynamicsAmount);

            float fltL = inFilterL.process(dryL);
            float fltR = inFilterR.process(dryR);

            float vlvL = velvetL.process(fltL);
            float vlvR = velvetR.process(fltR);

            float monoForER = (vlvL + vlvR) * 0.5f;

            inputDelayBuffer[inputDelayWritePos] = monoForER;
            int readIdx = inputDelayWritePos - currentPreDelaySamples;
            if (readIdx < 0) readIdx += inDelaySize;
            float delayedERInput = inputDelayBuffer[readIdx];
            inputDelayWritePos++;
            if (inputDelayWritePos >= inDelaySize) inputDelayWritePos = 0;

            float erL = 0.0f, erR = 0.0f;
            erEngine.process(delayedERInput, erL, erR);

            float delayOutputs[16] = { 0.0f };
            float feedbackInputs[16] = { 0.0f };

#pragma unroll
            for (int i = 0; i < 16; ++i) {
                float raw = channels[i].read(currentModDepth);
                delayOutputs[i] = channels[i].processFilter(raw);
            }

#pragma unroll
            for (int i = 0; i < 16; ++i) feedbackInputs[i] = delayOutputs[i];

            switch (currentShapeMode) {
            case 0: matrixHadamard(feedbackInputs); break;
            case 1: matrixHouseholder(feedbackInputs); break;
            case 2: matrixBlockPerm(feedbackInputs); break;
            case 3: matrixCylinder(feedbackInputs); break;
            case 4: matrixSparse(feedbackInputs); break;
            case 5: matrixMDS(feedbackInputs); break;
            case 6: matrixChaos(feedbackInputs); break;
            default: matrixHadamard(feedbackInputs); break;
            }

#pragma unroll
            for (int i = 0; i < 16; ++i) {
                float injS = 0.0f;
                if (i < 8) injS = vlvL * panInputL + vlvR * (1.0f - panInputL) * 0.5f;
                else       injS = vlvR * panInputR + vlvL * (1.0f - panInputR) * 0.5f;

                float wetIn = (i < 8) ? erL : erR;
                float injected = injS + wetIn * 0.5f;

                float sum = injected + feedbackInputs[i];
                if (currentDrive > 0.001f) sum = softSaturate(sum, currentDrive * 0.5f);
                sum = hardClip(sum);
                channels[i].push(sum);
            }

            float sumL = 0.0f, sumR = 0.0f;
#pragma unroll
            for (int i = 0; i < 8; ++i) sumL += delayOutputs[i];
#pragma unroll
            for (int i = 8; i < 16; ++i) sumR += delayOutputs[i];

            sumL *= 0.25f; sumR *= 0.25f;

            stereoSpreadBuffer[stereoSpreadWritePos] = sumR;
            int spreadIdx = stereoSpreadWritePos - stereoSpreadSamples;
            if (spreadIdx < 0) spreadIdx += 2048;
            spreadIdx &= 2047;
            float delayedR = stereoSpreadBuffer[spreadIdx];
            stereoSpreadWritePos = (stereoSpreadWritePos + 1) & 2047;

            float w = currentWidth;
            float mid = (sumL + delayedR) * 0.5f;
            float side = (sumL - delayedR) * 0.5f * w;
            if (w > 1.2f) side = sideHPF.process(side);

            float wetL = mid + side;
            float wetR = mid - side;

            wetL = dcBlockerL.process(wetL);
            wetR = dcBlockerR.process(wetR);
            wetL = outFilterL.process(wetL);
            wetR = outFilterR.process(wetR);

            float dryG = dryGainSmoother.getNext();
            float wetG = wetGainSmoother.getNext();
            float finalWetL = wetL + erL;
            float finalWetR = wetR + erR;

            finalWetL = tiltEQ_L.process(finalWetL);
            finalWetR = tiltEQ_R.process(finalWetR);

            finalWetL *= dynGain;
            finalWetR *= dynGain;

            float mixL = inL[n] * dryG + finalWetL * wetG;
            float mixR = inR[n] * dryG + finalWetR * wetG;

            outL[n] = std::clamp(mixL, -2.0f, 2.0f);
            outR[n] = std::clamp(mixR, -2.0f, 2.0f);
        }
    }

    RT60Data getEstimatedRT60() const { return lastRT60Data; }

private:
    std::vector<float> inputDelayBuffer;
    int inputDelayWritePos = 0;
    int currentPreDelaySamples = 0;
    ParameterSmoother dryGainSmoother;
    ParameterSmoother wetGainSmoother;
    std::vector<float> stereoSpreadBuffer;
    int stereoSpreadSamples = 24;
    int stereoSpreadWritePos = 0;
    HighQualityFilter inFilterL, inFilterR;
    HighQualityFilter outFilterL, outFilterR;
    DynamicsProcessor dynamicsProcessor;
    float currentDynamicsAmount = 0.0f;
    TiltEqualizer tiltEQ_L, tiltEQ_R;
    VelvetNoiseDiffuser velvetL, velvetR;
    OnePoleHighpass sideHPF;
    std::array<FDNChannel, FDN_CHANNELS> channels;
    EarlyReflections erEngine;
    DCBlocker dcBlockerL, dcBlockerR;
    double fs = 48000.0;
    float currentModDepth = 0.0f;
    float currentDrive = 0.0f;
    float currentWidth = 1.0f;
    float panInputL = 0.707f;
    float panInputR = 0.707f;
    float lastAvgDelay = 0.0f;
    RT60Data lastRT60Data;
    int currentShapeMode = 0;
    float calcT60(float g) const {
        float safeG = std::min(g, 0.9995f);
        if (safeG <= 0.001f) return 0.0f;
        float delaySec = lastAvgDelay / (float)fs;
        return -3.0f * delaySec / std::log10(safeG);
    }
};