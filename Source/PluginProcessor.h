/*
  ==============================================================================
    PluginProcessor.h
    Phase 182: Advanced Dynamics Header

    UPDATES:
    - Added dynThreshold, dynRatio, dynAttack, dynRelease to ReverbPreset.
    - Updated PhysicsState equality check.
    - Added atomic pointers for new parameters.
  ==============================================================================
*/
#pragma once

#include <JuceHeader.h>
#include "FDN_DSP.h"
#include <tuple>
#include <atomic>

// --- Data Structures ---

struct ReverbPreset {
    juce::String name;
    juce::String category;
    juce::String description;

    // Core Physics
    float width = 10.0f;
    float depth = 10.0f;
    float height = 5.0f;
    int matFloor = 0;
    int matCeil = 0;
    int matWallSide = 0;
    int matWallFB = 0;
    float absorption = 0.5f;

    // Modulation & Time
    float modRate = 0.5f;
    float modDepth = 0.2f;
    float predelay = 0.0f;
    float decay = 1.0f;

    // Environment
    float temp = 20.0f;
    float humidity = 50.0f;

    // Filters
    float inLC = 20.0f;
    float inHC = 20000.0f;
    float outLC = 20.0f;
    float outHC = 20000.0f;

    // Source & Mix
    float dist = 0.5f;
    float pan = 0.0f;
    float sourceHeight = 0.5f;
    float mix = 0.3f;

    // Character
    int roomShape = 0;
    float diffusion = 0.8f;
    float stereoWidth = 1.0f;
    float outputLevel = 1.0f;
    float drive = 0.0f;
    float density = 0.0f;

    // Advanced Features
    float dynamics = 0.0f; // Amount
    float tilt = 0.0f;

    // Phase 182: Dynamics Details
    float dynThreshold = -20.0f;
    float dynRatio = 2.0f;
    float dynAttack = 10.0f;
    float dynRelease = 100.0f;
};

struct PhysicsState {
    float w = 0.0f, d = 0.0f, h = 0.0f;
    int mf = 0, mc = 0, mws = 0, mwfb = 0;
    float abs = 0.0f, mRate = 0.0f, mDepth = 0.0f, pre = 0.0f, temp = 0.0f, hum = 0.0f, mix = 0.0f;
    float inLC = 0.0f, inHC = 0.0f, outLC = 0.0f, outHC = 0.0f;
    float dist = 0.0f, pan = 0.0f, srcH = 0.0f;
    int shape = 0;
    float diff = 0.0f, stW = 0.0f, outLvl = 0.0f, density = 0.0f, drive = 0.0f, decay = 0.0f;

    // Advanced
    float dynamics = 0.0f;
    float tilt = 0.0f;
    float dynThresh = 0.0f;
    float dynRatio = 0.0f;
    float dynAtt = 0.0f;
    float dynRel = 0.0f;

    int samples = 0;

    bool operator!=(const PhysicsState& other) const {
        return std::tie(w, d, h, mf, mc, mws, mwfb, abs, mRate, mDepth, pre, temp, hum, mix,
            inLC, inHC, outLC, outHC, dist, pan, srcH, shape, diff, stW, outLvl, density, drive, decay,
            dynamics, tilt, dynThresh, dynRatio, dynAtt, dynRel, samples)
            != std::tie(other.w, other.d, other.h, other.mf, other.mc, other.mws, other.mwfb, other.abs, other.mRate, other.mDepth, other.pre, other.temp, other.hum, other.mix,
                other.inLC, other.inHC, other.outLC, other.outHC, other.dist, other.pan, other.srcH, other.shape, other.diff, other.stW, other.outLvl, other.density, other.drive, other.decay,
                other.dynamics, other.tilt, other.dynThresh, other.dynRatio, other.dynAtt, other.dynRel, other.samples);
    }
};

class FdnReverbAudioProcessor : public juce::AudioProcessor
{
public:
    FdnReverbAudioProcessor();
    ~FdnReverbAudioProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

#ifndef JucePlugin_PreferredChannelConfigurations
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
#endif

    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;
    const juce::String getName() const override;
    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int index, const juce::String& newName) override;
    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    // Preset Management
    void initPresets();
    void loadPreset(int index);
    void saveUserPreset(const juce::String& name);
    void loadUserPreset(const juce::File& file);
    juce::File getUserPresetFolder() const;
    const std::vector<ReverbPreset>& getPresets() const { return presets; }
    int getCurrentPresetIndex() const { return currentPresetIndex; }
    juce::String getCurrentPresetName() const { return currentPresetName; }

    juce::AudioProcessorValueTreeState parameters;
    std::vector<ReverbPreset> presets;
    int currentPresetIndex = 0;
    juce::String currentPresetName = "Default";

    // Metering
    std::atomic<float> currentOutputLevel{ 0.0f };
    void triggerPanic() { panicTriggered.store(true); }

    RT60Data getRT60() const { return fdnEngine.getEstimatedRT60(); }

private:
    FDNEngine fdnEngine;

    std::unique_ptr<juce::dsp::Oversampling<float>> oversampling2x = nullptr;
    std::unique_ptr<juce::dsp::Oversampling<float>> oversampling4x = nullptr;
    juce::dsp::Oversampling<float>* currentOversampling = nullptr;
    int currentOversamplingFactor = 0;

    double storedSampleRate = 48000.0;
    int storedBlockSize = 512;
    float storedDspSampleRate = 48000.0f;
    int lastQualityFactor = 0;
    bool forceUpdate = true;
    std::atomic<bool> panicTriggered{ false };

    PhysicsState lastPhysicsState{};

    // Parameter Pointers (Fast Access)
    std::atomic<float>* widthParam = nullptr;
    std::atomic<float>* depthParam = nullptr;
    std::atomic<float>* heightParam = nullptr;
    std::atomic<float>* matFloorParam = nullptr;
    std::atomic<float>* matCeilParam = nullptr;
    std::atomic<float>* matWallSideParam = nullptr;
    std::atomic<float>* matWallFBParam = nullptr;
    std::atomic<float>* absorbParam = nullptr;
    std::atomic<float>* modRateParam = nullptr;
    std::atomic<float>* modDepthParam = nullptr;
    std::atomic<float>* mixParam = nullptr;
    std::atomic<float>* tempParam = nullptr;
    std::atomic<float>* humParam = nullptr;

    std::atomic<float>* inLCParam = nullptr;
    std::atomic<float>* inHCParam = nullptr;
    std::atomic<float>* outLCParam = nullptr;
    std::atomic<float>* outHCParam = nullptr;

    std::atomic<float>* distParam = nullptr;
    std::atomic<float>* panParam = nullptr;
    std::atomic<float>* heightSourceParam = nullptr;
    std::atomic<float>* shapeParam = nullptr;
    std::atomic<float>* diffParam = nullptr;
    std::atomic<float>* widthStereoParam = nullptr;
    std::atomic<float>* levelParam = nullptr;

    std::atomic<float>* qualityParam = nullptr;
    std::atomic<float>* driveParam = nullptr;
    std::atomic<float>* densityParam = nullptr;
    std::atomic<float>* decayParam = nullptr;

    // New Pointers
    std::atomic<float>* dynamicsParam = nullptr;
    std::atomic<float>* tiltParam = nullptr;

    // Phase 182: Advanced Dynamics Pointers
    std::atomic<float>* dynThreshParam = nullptr;
    std::atomic<float>* dynRatioParam = nullptr;
    std::atomic<float>* dynAttackParam = nullptr;
    std::atomic<float>* dynReleaseParam = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FdnReverbAudioProcessor)
};