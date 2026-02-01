/*
  ==============================================================================
    PluginProcessor.cpp
    Phase 182: Advanced Dynamics Implementation - PART 1/3

    CONTENTS:
    - Parameter Layout (Added Thresh, Ratio, Attack, Release)
    - Constructor & Initialization
  ==============================================================================
*/
#include "PluginProcessor.h"
#include "PluginEditor.h"

static juce::String utf8(const char* text) {
    if (text == nullptr) return {};
    return juce::String::fromUTF8(text);
}

// ==============================================================================
// 1. PARAMETER LAYOUT
// ==============================================================================
static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout() {
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    juce::StringArray matNames;
    // Basic (0-12)
    matNames.add(utf8(u8"Concrete Rough (粗コンクリ)"));
    matNames.add(utf8(u8"Concrete Block (ブロック)"));
    matNames.add(utf8(u8"Wood Varnished (ニス塗木材)"));
    matNames.add(utf8(u8"Wood Parquet (寄木細工)"));
    matNames.add(utf8(u8"Carpet Heavy (厚手絨毯)"));
    matNames.add(utf8(u8"Curtain Velvet (ベルベット)"));
    matNames.add(utf8(u8"Acoustic Tile (吸音タイル)"));
    matNames.add(utf8(u8"Brick Wall (レンガ壁)"));
    matNames.add(utf8(u8"Glass Window (ガラス窓)"));
    matNames.add(utf8(u8"Metal Sheet (金属板)"));
    matNames.add(utf8(u8"Water Surface (水面)"));
    matNames.add(utf8(u8"Marble Floor (大理石)"));
    matNames.add(utf8(u8"Space Void (虚空)"));
    // Bio (13-21)
    matNames.add(utf8(u8"Vocal Tract (声道)"));
    matNames.add(utf8(u8"Tatami (畳)"));
    matNames.add(utf8(u8"Acrylic (アクリル)"));
    matNames.add(utf8(u8"Carbon Fiber (カーボン)"));
    matNames.add(utf8(u8"Fresh Snow (新雪)"));
    matNames.add(utf8(u8"Forest Floor (森林)"));
    matNames.add(utf8(u8"Cave (洞窟)"));
    matNames.add(utf8(u8"Muscle Tissue (筋肉)"));
    matNames.add(utf8(u8"Blubber (脂肪)"));
    // Transmissive (22-25)
    matNames.add(utf8(u8"Shoji (障子)"));
    matNames.add(utf8(u8"Double Glazing (二重窓)"));
    matNames.add(utf8(u8"Bookshelf (本棚)"));
    matNames.add(utf8(u8"Heavy Curtain (暗幕)"));
    // Nature (26-29)
    matNames.add(utf8(u8"Ice Sheet (氷原)"));
    matNames.add(utf8(u8"Magma (溶岩)"));
    matNames.add(utf8(u8"Sand Dune (砂丘)"));
    matNames.add(utf8(u8"Swamp (沼地)"));
    // Sci-Fi (30-33)
    matNames.add(utf8(u8"Aerogel (エアロゲル)"));
    matNames.add(utf8(u8"Plasma Field (プラズマ)"));
    matNames.add(utf8(u8"Neutron Star (中性子星)"));
    matNames.add(utf8(u8"Force Field (力場)"));

    auto addCenteredFloat = [&](const juce::String& id, const juce::String& name,
        float min, float max, float def, float centre,
        const juce::String& unit, int decimals = 1) {
            juce::NormalisableRange<float> range(min, max);
            range.setSkewForCentre(centre);
            params.push_back(std::make_unique<juce::AudioParameterFloat>(
                id, name, range, def,
                juce::String(),
                juce::AudioProcessorParameter::Category::genericParameter,
                [unit, decimals](float value, int) { return juce::String(value, decimals) + " " + unit; },
                [](const juce::String& text) { return text.getFloatValue(); }
            ));
        };

    auto addPercent = [&](const juce::String& id, const juce::String& name, float min, float max, float def) {
        params.push_back(std::make_unique<juce::AudioParameterFloat>(id, name,
            juce::NormalisableRange<float>(min, max), def,
            juce::String(),
            juce::AudioProcessorParameter::Category::genericParameter,
            [](float value, int) { return juce::String(value * 100.0f, 1) + " %"; },
            [](const juce::String& text) { return text.getFloatValue() / 100.0f; }
        ));
        };

    addCenteredFloat("room_width", utf8(u8"Width (幅)"), 2.0f, 300.0f, 10.0f, 20.0f, "m");
    addCenteredFloat("room_depth", utf8(u8"Depth (奥行)"), 2.0f, 300.0f, 15.0f, 20.0f, "m");
    addCenteredFloat("room_height", utf8(u8"Height (高さ)"), 2.0f, 300.0f, 5.0f, 10.0f, "m");

    params.push_back(std::make_unique<juce::AudioParameterChoice>("mat_floor", utf8(u8"Floor (床材)"), matNames, 2));
    params.push_back(std::make_unique<juce::AudioParameterChoice>("mat_ceil", utf8(u8"Ceiling (天井材)"), matNames, 6));
    params.push_back(std::make_unique<juce::AudioParameterChoice>("mat_wall_s", utf8(u8"Wall Side (横壁)"), matNames, 0));
    params.push_back(std::make_unique<juce::AudioParameterChoice>("mat_wall_fb", utf8(u8"Wall F/B (前後壁)"), matNames, 0));

    addPercent("absorption", utf8(u8"Absorb (吸音率)"), 0.0f, 1.0f, 0.5f);

    addCenteredFloat("mod_rate", utf8(u8"Mod Rate (揺らぎ速度)"), 0.0f, 2.0f, 0.5f, 0.5f, "Hz", 2);

    {
        juce::NormalisableRange<float> range(0.0f, 1.0f);
        range.skew = 0.5f;
        params.push_back(std::make_unique<juce::AudioParameterFloat>("mod_depth", utf8(u8"Mod Depth (揺らぎ深さ)"),
            range, 0.2f,
            juce::String(),
            juce::AudioProcessorParameter::Category::genericParameter,
            [](float value, int) { return juce::String(value * 100.0f, 1) + " %"; },
            [](const juce::String& text) { return text.getFloatValue() / 100.0f; }
        ));
    }

    {
        juce::NormalisableRange<float> range(0.0f, 500.0f);
        range.skew = 0.3f;
        params.push_back(std::make_unique<juce::AudioParameterFloat>("predelay", utf8(u8"Pre-Delay (初期遅延)"),
            range, 0.0f,
            juce::String(),
            juce::AudioProcessorParameter::Category::genericParameter,
            [](float value, int) { return juce::String(value, 1) + " ms"; },
            [](const juce::String& text) { return text.getFloatValue(); }
        ));
    }

    params.push_back(std::make_unique<juce::AudioParameterFloat>("dry_wet", utf8(u8"Mix (混合比)"),
        juce::NormalisableRange<float>(0.0f, 1.0f), 0.3f,
        juce::String(),
        juce::AudioProcessorParameter::Category::genericParameter,
        [](float value, int) { return juce::String(value * 100.0f, 1) + " %"; },
        [](const juce::String& text) { return text.getFloatValue() / 100.0f; }
    ));

    addCenteredFloat("temp", utf8(u8"Temp (気温)"), -100.0f, 200.0f, 20.0f, 20.0f, "C");
    params.push_back(std::make_unique<juce::AudioParameterFloat>("humidity", utf8(u8"Humidity (湿度)"),
        juce::NormalisableRange<float>(10.0f, 90.0f), 50.0f,
        juce::String(),
        juce::AudioProcessorParameter::Category::genericParameter,
        [](float value, int) { return juce::String(value, 1) + " %"; },
        [](const juce::String& text) { return text.getFloatValue(); }
    ));

    addCenteredFloat("in_lc", utf8(u8"In LowCut"), 20.0f, 1000.0f, 20.0f, 100.0f, "Hz", 0);
    addCenteredFloat("in_hc", utf8(u8"In HighCut"), 1000.0f, 20000.0f, 20000.0f, 8000.0f, "Hz", 0);
    addCenteredFloat("out_lc", utf8(u8"Out LowCut"), 20.0f, 1000.0f, 20.0f, 100.0f, "Hz", 0);
    addCenteredFloat("out_hc", utf8(u8"Out HighCut"), 1000.0f, 20000.0f, 20000.0f, 8000.0f, "Hz", 0);

    addPercent("dist", utf8(u8"Distance (距離)"), 0.0f, 1.0f, 0.5f);

    params.push_back(std::make_unique<juce::AudioParameterFloat>("pan", utf8(u8"Pan (定位)"),
        juce::NormalisableRange<float>(-1.0f, 1.0f), 0.0f,
        juce::String(),
        juce::AudioProcessorParameter::Category::genericParameter,
        [](float value, int) {
            if (std::abs(value) < 0.01f) return juce::String("C");
            if (value < 0.0f) return juce::String(std::abs(value) * 100.0f, 0) + " L";
            return juce::String(value * 100.0f, 0) + " R";
        },
        [](const juce::String& text) {
            float v = text.getFloatValue();
            if (text.contains("L")) return -v / 100.0f;
            if (text.contains("R")) return v / 100.0f;
            return 0.0f;
        }
    ));

    addPercent("src_height", utf8(u8"Src Height (%)"), 0.0f, 1.0f, 0.5f);

    juce::StringArray shapes;
    shapes.add(utf8(u8"Shoe-box (箱型)"));
    shapes.add(utf8(u8"Dome (ドーム)"));
    shapes.add(utf8(u8"Fan (扇型)"));
    shapes.add(utf8(u8"Cylinder (円筒)"));
    shapes.add(utf8(u8"Pyramid (ピラミッド)"));
    shapes.add(utf8(u8"Tesseract (4次元)"));
    shapes.add(utf8(u8"Chaos (カオス)"));

    params.push_back(std::make_unique<juce::AudioParameterChoice>("shape", utf8(u8"Shape (形状)"), shapes, 0));

    addPercent("diffusion", utf8(u8"Diffusion (拡散)"), 0.0f, 1.0f, 0.8f);

    params.push_back(std::make_unique<juce::AudioParameterFloat>("width_st", utf8(u8"Stereo W (広がり)"),
        juce::NormalisableRange<float>(0.0f, 2.0f), 1.0f,
        juce::String(),
        juce::AudioProcessorParameter::Category::genericParameter,
        [](float value, int) { return juce::String(value * 100.0f, 1) + " %"; },
        [](const juce::String& text) { return text.getFloatValue() / 100.0f; }
    ));

    addPercent("level", utf8(u8"Level (出力)"), 0.0f, 2.0f, 1.0f);

    juce::StringArray qualities;
    qualities.add("Off"); qualities.add("2x"); qualities.add("4x");
    params.push_back(std::make_unique<juce::AudioParameterChoice>("quality", utf8(u8"Quality (品質)"), qualities, 0));

    addPercent("drive", utf8(u8"Drive (歪み)"), 0.0f, 1.0f, 0.0f);
    addPercent("density", utf8(u8"Density (密度)"), 0.0f, 1.0f, 0.0f);

    auto decayRange = juce::NormalisableRange<float>(0.0f, 5.0f);
    decayRange.setSkewForCentre(1.0f);
    params.push_back(std::make_unique<juce::AudioParameterFloat>("decay", utf8(u8"Decay (残響時間)"),
        decayRange, 1.0f,
        juce::String(),
        juce::AudioProcessorParameter::Category::genericParameter,
        [](float value, int) { return juce::String(value * 100.0f, 1) + " %"; },
        [](const juce::String& text) { return text.getFloatValue() / 100.0f; }
    ));

    // Dynamics Amount
    params.push_back(std::make_unique<juce::AudioParameterFloat>("dynamics", utf8(u8"Dynamics (動特性)"),
        juce::NormalisableRange<float>(-1.0f, 1.0f), 0.0f,
        juce::String(),
        juce::AudioProcessorParameter::Category::genericParameter,
        [](float value, int) {
            if (std::abs(value) < 0.01f) return juce::String("Off");
            if (value < 0.0f) return juce::String("Duck ") + juce::String(std::abs(value) * 100.0f, 0) + "%";
            return juce::String("Bloom ") + juce::String(value * 100.0f, 0) + "%";
        },
        [](const juce::String& text) { return 0.0f; }
    ));

    // Tilt EQ
    params.push_back(std::make_unique<juce::AudioParameterFloat>("tilt", utf8(u8"Tilt EQ (音色)"),
        juce::NormalisableRange<float>(-6.0f, 6.0f), 0.0f,
        juce::String(),
        juce::AudioProcessorParameter::Category::genericParameter,
        [](float value, int) { return (value > 0 ? "+" : "") + juce::String(value, 1) + " dB"; },
        [](const juce::String& text) { return text.getFloatValue(); }
    ));

    // Phase 182: Advanced Dynamics Parameters
    params.push_back(std::make_unique<juce::AudioParameterFloat>("dyn_thresh", utf8(u8"Threshold"),
        juce::NormalisableRange<float>(-60.0f, 0.0f), -20.0f,
        juce::String(),
        juce::AudioProcessorParameter::Category::genericParameter,
        [](float value, int) { return juce::String(value, 1) + " dB"; },
        [](const juce::String& text) { return text.getFloatValue(); }
    ));

    params.push_back(std::make_unique<juce::AudioParameterFloat>("dyn_ratio", utf8(u8"Ratio"),
        juce::NormalisableRange<float>(1.0f, 20.0f), 2.0f,
        juce::String(),
        juce::AudioProcessorParameter::Category::genericParameter,
        [](float value, int) { return "1:" + juce::String(value, 1); },
        [](const juce::String& text) { return text.getFloatValue(); }
    ));

    params.push_back(std::make_unique<juce::AudioParameterFloat>("dyn_attack", utf8(u8"Attack"),
        juce::NormalisableRange<float>(1.0f, 200.0f), 10.0f,
        juce::String(),
        juce::AudioProcessorParameter::Category::genericParameter,
        [](float value, int) { return juce::String(value, 1) + " ms"; },
        [](const juce::String& text) { return text.getFloatValue(); }
    ));

    params.push_back(std::make_unique<juce::AudioParameterFloat>("dyn_release", utf8(u8"Release"),
        juce::NormalisableRange<float>(10.0f, 1000.0f), 100.0f,
        juce::String(),
        juce::AudioProcessorParameter::Category::genericParameter,
        [](float value, int) { return juce::String(value, 1) + " ms"; },
        [](const juce::String& text) { return text.getFloatValue(); }
    ));

    return { params.begin(), params.end() };
}

// ==============================================================================
// 2. CONSTRUCTOR
// ==============================================================================
FdnReverbAudioProcessor::FdnReverbAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
    : AudioProcessor(BusesProperties().withInput("Input", juce::AudioChannelSet::stereo(), true).withOutput("Output", juce::AudioChannelSet::stereo(), true)),
    parameters(*this, nullptr, "PARAMETERS", createParameterLayout())
#endif
{
    widthParam = parameters.getRawParameterValue("room_width");
    depthParam = parameters.getRawParameterValue("room_depth");
    heightParam = parameters.getRawParameterValue("room_height");
    matFloorParam = parameters.getRawParameterValue("mat_floor");
    matCeilParam = parameters.getRawParameterValue("mat_ceil");
    matWallSideParam = parameters.getRawParameterValue("mat_wall_s");
    matWallFBParam = parameters.getRawParameterValue("mat_wall_fb");
    absorbParam = parameters.getRawParameterValue("absorption");
    modRateParam = parameters.getRawParameterValue("mod_rate");
    modDepthParam = parameters.getRawParameterValue("mod_depth");
    mixParam = parameters.getRawParameterValue("dry_wet");
    tempParam = parameters.getRawParameterValue("temp");
    humParam = parameters.getRawParameterValue("humidity");

    inLCParam = parameters.getRawParameterValue("in_lc");
    inHCParam = parameters.getRawParameterValue("in_hc");
    outLCParam = parameters.getRawParameterValue("out_lc");
    outHCParam = parameters.getRawParameterValue("out_hc");

    distParam = parameters.getRawParameterValue("dist");
    panParam = parameters.getRawParameterValue("pan");
    heightSourceParam = parameters.getRawParameterValue("src_height");
    shapeParam = parameters.getRawParameterValue("shape");
    diffParam = parameters.getRawParameterValue("diffusion");
    widthStereoParam = parameters.getRawParameterValue("width_st");
    levelParam = parameters.getRawParameterValue("level");

    qualityParam = parameters.getRawParameterValue("quality");
    driveParam = parameters.getRawParameterValue("drive");
    densityParam = parameters.getRawParameterValue("density");

    decayParam = parameters.getRawParameterValue("decay");

    // Standard Params
    dynamicsParam = parameters.getRawParameterValue("dynamics");
    tiltParam = parameters.getRawParameterValue("tilt");

    // Phase 182: Advanced Params
    dynThreshParam = parameters.getRawParameterValue("dyn_thresh");
    dynRatioParam = parameters.getRawParameterValue("dyn_ratio");
    dynAttackParam = parameters.getRawParameterValue("dyn_attack");
    dynReleaseParam = parameters.getRawParameterValue("dyn_release");

    initPresets();
}

FdnReverbAudioProcessor::~FdnReverbAudioProcessor() {
    oversampling2x.reset();
    oversampling4x.reset();
}
void FdnReverbAudioProcessor::initPresets() {
    presets.clear();

    // Helper lambda to safely construct presets regardless of struct memory layout
    auto addPreset = [&](
        const juce::String& name, const juce::String& cat, const juce::String& desc,
        float w, float d, float h, int mf, int mc, int mws, int mwfb, float abs,
        float rate, float depth, float mix,
        float temp, float hum, float inlc, float inhc, float outlc, float outhc,
        float dist, float pan, float srcH,
        int shape, float diff, float stw, float lvl, float drive, float dens,
        float decay, float pre, float dyn, float tilt,
        float th, float rat, float att, float rel)
        {
            ReverbPreset p;
            p.name = name; p.category = cat; p.description = desc;
            p.width = w; p.depth = d; p.height = h;
            p.matFloor = mf; p.matCeil = mc; p.matWallSide = mws; p.matWallFB = mwfb;
            p.absorption = abs;
            p.modRate = rate; p.modDepth = depth; p.mix = mix;
            p.temp = temp; p.humidity = hum;
            p.inLC = inlc; p.inHC = inhc; p.outLC = outlc; p.outHC = outhc;
            p.dist = dist; p.pan = pan; p.sourceHeight = srcH;
            p.roomShape = shape; p.diffusion = diff; p.stereoWidth = stw; p.outputLevel = lvl;
            p.drive = drive; p.density = dens;
            p.decay = decay; p.predelay = pre;
            p.dynamics = dyn; p.tilt = tilt;
            p.dynThreshold = th; p.dynRatio = rat; p.dynAttack = att; p.dynRelease = rel;
            presets.push_back(p);
        };

    // =========================================================================================
    // 1. 00_Basic (The Essential 7 Types)
    // Rule: Mix 10-20%, Dist <= 30%, Diff <= 30%, Decay 10-45%
    // =========================================================================================

    // Room 1: Tight (Decay 10%)
    addPreset("Room 1 (Tight)", "00_Basic", utf8(u8"Decay 10%。非常にデッドでタイトなブース。ナレーションやドライなドラムに。"),
        3.0f, 2.5f, 2.2f, 4, 6, 6, 6, 0.45f,
        0.0f, 0.0f, 0.15f, // Rate, Depth, Mix
        20.0f, 50.0f, 20.0f, 15000.0f, 80.0f, 12000.0f, // Temp, Hum, Filters
        0.1f, 0.0f, 0.2f, // Dist, Pan, SrcH
        0, 0.1f, 1.0f, 1.0f, 0.0f, 0.1f, // Shape, Diff, StW, Lvl, Drive, Dens
        0.10f, 0.0f, 0.0f, 0.0f, // Decay, Pre, Dyn, Tilt
        -20.0f, 2.0f, 10.0f, 100.0f); // Thresh, Ratio, Att, Rel

    // Room 2: Studio (Decay 15%)
    addPreset("Room 2 (Studio)", "00_Basic", utf8(u8"Decay 15%。木製床の標準的なスタジオルーム。ギターやボーカルに。"),
        6.0f, 8.0f, 3.5f, 2, 6, 2, 2, 0.40f,
        0.2f, 0.1f, 0.15f,
        22.0f, 45.0f, 50.0f, 20000.0f, 40.0f, 18000.0f,
        0.2f, 0.0f, 0.2f,
        0, 0.2f, 1.0f, 1.0f, 0.0f, 0.2f,
        0.15f, 5.0f, 0.0f, 0.0f,
        -20.0f, 2.0f, 10.0f, 100.0f);

    // Room 3: Chamber (Decay 20%)
    addPreset("Room 3 (Chamber)", "00_Basic", utf8(u8"Decay 20%。石壁の反射が明るいチェンバー。パーカッションに。"),
        5.0f, 7.0f, 4.0f, 1, 1, 7, 7, 0.35f,
        0.3f, 0.1f, 0.20f,
        18.0f, 60.0f, 20.0f, 20000.0f, 50.0f, 18000.0f,
        0.25f, 0.0f, 0.2f,
        0, 0.25f, 1.0f, 1.0f, 0.1f, 0.3f,
        0.20f, 12.0f, 0.0f, 0.0f,
        -20.0f, 2.0f, 10.0f, 100.0f);

    // Hall 1: Recital (Decay 20%)
    addPreset("Hall 1 (Recital)", "00_Basic", utf8(u8"Decay 20%。小規模で親密なリサイタルホール。ピアノや弦楽に。"),
        15.0f, 20.0f, 8.0f, 3, 2, 2, 2, 0.30f,
        0.4f, 0.2f, 0.20f,
        22.0f, 50.0f, 20.0f, 20000.0f, 30.0f, 14000.0f,
        0.3f, 0.0f, 0.15f,
        2, 0.25f, 1.0f, 1.0f, 0.0f, 0.25f,
        0.20f, 20.0f, 0.0f, 0.0f,
        -20.0f, 2.0f, 10.0f, 100.0f);

    // Hall 2: Symphonic (Decay 30%)
    addPreset("Hall 2 (Symphonic)", "00_Basic", utf8(u8"Decay 30%。豊かな低域を持つ大ホール。オーケストラに。"),
        30.0f, 45.0f, 18.0f, 3, 2, 2, 2, 0.25f,
        0.5f, 0.2f, 0.20f,
        20.0f, 55.0f, 20.0f, 18000.0f, 20.0f, 12000.0f,
        0.3f, 0.0f, 0.2f,
        0, 0.30f, 1.0f, 1.0f, 0.0f, 0.3f,
        0.30f, 35.0f, 0.0f, 0.0f,
        -20.0f, 2.0f, 10.0f, 100.0f);

    // Plate 1: Vocal (Decay 30%)
    addPreset("Plate 1 (Vocal)", "00_Basic", utf8(u8"Decay 30%。ボーカル用。高密度で煌びやかなプレート。"),
        4.0f, 3.0f, 0.2f, 9, 9, 9, 9, 0.30f,
        0.6f, 0.1f, 0.20f,
        25.0f, 40.0f, 50.0f, 20000.0f, 100.0f, 16000.0f,
        0.1f, 0.0f, 0.3f,
        3, 0.30f, 1.0f, 1.0f, 0.4f, 0.5f,
        0.30f, 0.0f, 0.0f, 0.0f,
        -20.0f, 2.0f, 10.0f, 100.0f);

    // Plate 2: Long (Decay 45%)
    addPreset("Plate 2 (Long)", "00_Basic", utf8(u8"Decay 45%。長い余韻を持つダークなプレート。バラードに。"),
        6.0f, 4.0f, 0.5f, 9, 9, 9, 9, 0.25f,
        0.8f, 0.2f, 0.20f,
        20.0f, 50.0f, 50.0f, 15000.0f, 80.0f, 10000.0f,
        0.1f, 0.0f, 0.3f,
        3, 0.30f, 1.0f, 1.0f, 0.3f, 0.5f,
        0.45f, 10.0f, 0.0f, 0.0f,
        -20.0f, 2.0f, 10.0f, 100.0f);

    // ==============================================================================
    // 2. 01_Small Spaces
    // ==============================================================================
    addPreset("Vocal Booth (Dry)", "01_Small", utf8(u8"極めてデッドな録音ブース。"),
        2.0f, 2.0f, 2.2f, 4, 6, 6, 6, 0.50f,
        0.0f, 0.0f, 0.15f,
        22.0f, 50.0f, 100.0f, 16000.0f, 100.0f, 14000.0f,
        0.1f, 0.0f, 0.2f,
        0, 0.1f, 1.0f, 1.0f, 0.0f, 0.1f,
        0.10f, 0.0f, 0.0f, 0.0f, -20.0f, 2.0f, 10.0f, 100.0f);

    addPreset("Tiled Bathroom", "01_Small", utf8(u8"タイル張りのバスルーム。"),
        2.5f, 3.0f, 2.5f, 11, 11, 11, 11, 0.25f,
        0.0f, 0.0f, 0.15f,
        25.0f, 80.0f, 50.0f, 20000.0f, 50.0f, 18000.0f,
        0.2f, 0.0f, 0.2f,
        0, 0.3f, 1.0f, 1.0f, 0.0f, 0.2f,
        0.25f, 5.0f, 0.0f, 0.0f, -20.0f, 2.0f, 10.0f, 100.0f);

    addPreset("Living Room", "01_Small", utf8(u8"一般的なリビング。"),
        5.0f, 6.0f, 2.4f, 3, 6, 24, 8, 0.40f,
        0.1f, 0.1f, 0.15f,
        22.0f, 50.0f, 20.0f, 18000.0f, 50.0f, 15000.0f,
        0.2f, 0.0f, 0.2f,
        0, 0.2f, 1.0f, 1.0f, 0.0f, 0.1f,
        0.20f, 8.0f, 0.0f, 0.0f, -20.0f, 2.0f, 10.0f, 100.0f);

    addPreset("Car Interior", "01_Small", utf8(u8"車内。ブーミー。"),
        1.8f, 2.5f, 1.2f, 4, 5, 8, 8, 0.45f,
        0.0f, 0.0f, 0.20f,
        20.0f, 50.0f, 150.0f, 10000.0f, 150.0f, 8000.0f,
        0.1f, 0.0f, 0.1f,
        0, 0.1f, 1.0f, 1.0f, 0.0f, 0.2f,
        0.10f, 0.0f, 0.0f, 0.0f, -20.0f, 2.0f, 10.0f, 100.0f);

    addPreset("Stone Cellar", "01_Small", utf8(u8"石造りの地下室。"),
        4.0f, 5.0f, 2.2f, 1, 1, 1, 1, 0.30f,
        0.1f, 0.1f, 0.20f,
        12.0f, 70.0f, 40.0f, 16000.0f, 40.0f, 12000.0f,
        0.2f, 0.0f, 0.2f,
        0, 0.25f, 1.0f, 1.0f, 0.0f, 0.2f,
        0.25f, 15.0f, 0.0f, 0.0f, -20.0f, 2.0f, 10.0f, 100.0f);

    addPreset("Wooden Sauna", "01_Small", utf8(u8"サウナ。ドライ。"),
        3.0f, 3.0f, 2.2f, 2, 2, 2, 2, 0.35f,
        0.0f, 0.0f, 0.15f,
        80.0f, 10.0f, 50.0f, 18000.0f, 50.0f, 16000.0f,
        0.2f, 0.0f, 0.2f,
        0, 0.2f, 1.0f, 1.0f, 0.0f, 0.2f,
        0.15f, 5.0f, 0.0f, 0.0f, -20.0f, 2.0f, 10.0f, 100.0f);

    addPreset("Elevator", "01_Small", utf8(u8"エレベーター。金属的。"),
        2.0f, 2.0f, 2.5f, 9, 9, 9, 9, 0.20f,
        0.0f, 0.0f, 0.20f,
        20.0f, 50.0f, 100.0f, 15000.0f, 150.0f, 12000.0f,
        0.15f, 0.0f, 0.2f,
        3, 0.3f, 1.0f, 1.0f, 0.1f, 0.3f,
        0.20f, 2.0f, 0.0f, 0.0f, -20.0f, 2.0f, 10.0f, 100.0f);

    addPreset("Stairwell", "01_Small", utf8(u8"階段の踊り場。"),
        3.0f, 6.0f, 10.0f, 0, 0, 0, 0, 0.20f,
        0.0f, 0.0f, 0.20f,
        18.0f, 50.0f, 50.0f, 18000.0f, 80.0f, 15000.0f,
        0.3f, 0.0f, 0.3f,
        3, 0.3f, 1.0f, 1.0f, 0.0f, 0.2f,
        0.30f, 15.0f, 0.0f, 0.0f, -20.0f, 2.0f, 10.0f, 100.0f);

    addPreset("Hallway", "01_Small", utf8(u8"学校の廊下。"),
        2.5f, 20.0f, 3.0f, 11, 6, 0, 0, 0.30f,
        0.0f, 0.0f, 0.20f,
        20.0f, 50.0f, 50.0f, 16000.0f, 50.0f, 14000.0f,
        0.3f, 0.0f, 0.2f,
        0, 0.25f, 1.0f, 1.0f, 0.0f, 0.3f,
        0.35f, 20.0f, 0.0f, 0.0f, -20.0f, 2.0f, 10.0f, 100.0f);

    addPreset("Kitchen", "01_Small", utf8(u8"キッチン。"),
        4.0f, 5.0f, 2.4f, 11, 6, 8, 9, 0.30f,
        0.0f, 0.0f, 0.15f,
        22.0f, 60.0f, 50.0f, 18000.0f, 50.0f, 16000.0f,
        0.2f, 0.0f, 0.2f,
        0, 0.25f, 1.0f, 1.0f, 0.0f, 0.2f,
        0.15f, 5.0f, 0.0f, 0.0f, -20.0f, 2.0f, 10.0f, 100.0f);

    addPreset("Garage", "01_Small", utf8(u8"ガレージ。"),
        6.0f, 8.0f, 2.8f, 0, 0, 1, 9, 0.30f,
        0.0f, 0.0f, 0.15f,
        15.0f, 60.0f, 40.0f, 15000.0f, 40.0f, 12000.0f,
        0.2f, 0.0f, 0.2f,
        0, 0.3f, 1.0f, 1.0f, 0.0f, 0.3f,
        0.20f, 10.0f, 0.0f, 0.0f, -20.0f, 2.0f, 10.0f, 100.0f);

    addPreset("Office", "01_Small", utf8(u8"オフィス。"),
        15.0f, 20.0f, 2.8f, 4, 6, 8, 8, 0.45f,
        0.0f, 0.0f, 0.15f,
        22.0f, 40.0f, 50.0f, 15000.0f, 50.0f, 12000.0f,
        0.2f, 0.0f, 0.2f,
        0, 0.15f, 1.0f, 1.0f, 0.0f, 0.2f,
        0.15f, 15.0f, 0.0f, 0.0f, -20.0f, 2.0f, 10.0f, 100.0f);

    addPreset("Classroom", "01_Small", utf8(u8"教室。"),
        8.0f, 10.0f, 3.0f, 3, 6, 0, 0, 0.35f,
        0.0f, 0.0f, 0.15f,
        20.0f, 50.0f, 50.0f, 16000.0f, 50.0f, 14000.0f,
        0.2f, 0.0f, 0.2f,
        0, 0.20f, 1.0f, 1.0f, 0.0f, 0.2f,
        0.20f, 10.0f, 0.0f, 0.0f, -20.0f, 2.0f, 10.0f, 100.0f);

    addPreset("Library", "01_Small", utf8(u8"図書館。"),
        20.0f, 30.0f, 4.0f, 4, 6, 24, 24, 0.45f,
        0.0f, 0.0f, 0.15f,
        20.0f, 40.0f, 50.0f, 14000.0f, 50.0f, 10000.0f,
        0.3f, 0.0f, 0.2f,
        0, 0.25f, 1.0f, 1.0f, 0.0f, 0.3f,
        0.25f, 15.0f, 0.0f, 0.0f, -20.0f, 2.0f, 10.0f, 100.0f);

    addPreset("Tatami Room", "01_Small", utf8(u8"和室。"),
        6.0f, 6.0f, 2.4f, 14, 2, 22, 22, 0.45f,
        0.0f, 0.0f, 0.15f,
        20.0f, 60.0f, 60.0f, 12000.0f, 60.0f, 10000.0f,
        0.2f, 0.0f, 0.1f,
        0, 0.1f, 1.0f, 1.0f, 0.0f, 0.1f,
        0.10f, 5.0f, 0.0f, 0.0f, -20.0f, 2.0f, 10.0f, 100.0f);

    addPreset("Glass Box", "01_Small", utf8(u8"全面ガラス張り。"),
        4.0f, 4.0f, 3.0f, 8, 8, 8, 8, 0.15f,
        0.0f, 0.0f, 0.15f,
        20.0f, 40.0f, 100.0f, 20000.0f, 100.0f, 18000.0f,
        0.1f, 0.0f, 0.2f,
        0, 0.1f, 1.0f, 1.0f, 0.0f, 0.1f,
        0.15f, 5.0f, 0.0f, 0.0f, -20.0f, 2.0f, 10.0f, 100.0f);

    addPreset("Drum Booth (Tight)", "01_Small", utf8(u8"ドラムブース。"),
        3.0f, 4.0f, 2.5f, 2, 6, 6, 6, 0.40f,
        0.0f, 0.0f, 0.15f,
        22.0f, 50.0f, 40.0f, 18000.0f, 40.0f, 16000.0f,
        0.1f, 0.0f, 0.2f,
        0, 0.15f, 1.0f, 1.0f, 0.0f, 0.2f,
        0.10f, 0.0f, 0.0f, 0.0f, -20.0f, 2.0f, 10.0f, 100.0f);

    addPreset("Piano Room (Home)", "01_Small", utf8(u8"自宅のピアノ室。"),
        4.0f, 5.0f, 2.6f, 3, 6, 2, 2, 0.35f,
        0.0f, 0.0f, 0.15f,
        22.0f, 50.0f, 30.0f, 18000.0f, 30.0f, 16000.0f,
        0.2f, 0.0f, 0.2f,
        0, 0.20f, 1.0f, 1.0f, 0.0f, 0.2f,
        0.15f, 5.0f, 0.0f, 0.0f, -20.0f, 2.0f, 10.0f, 100.0f);

    // ==============================================================================
    // 3. 02_Studios
    // ==============================================================================
    addPreset("Abbey Road St1", "02_Studios", utf8(u8"ロンドン。大会場。"),
        18.0f, 25.0f, 12.0f, 3, 6, 6, 6, 0.35f,
        0.2f, 0.1f, 0.20f,
        15.0f, 60.0f, 20.0f, 20000.0f, 30.0f, 16000.0f,
        0.3f, 0.0f, 0.2f,
        0, 0.30f, 1.0f, 1.0f, 0.0f, 0.3f,
        0.35f, 30.0f, 0.0f, 0.0f, -20.0f, 2.0f, 10.0f, 100.0f);

    addPreset("Abbey Road St2", "02_Studios", utf8(u8"ビートルズの部屋。"),
        13.0f, 15.0f, 7.0f, 3, 6, 7, 7, 0.40f,
        0.1f, 0.1f, 0.20f,
        15.0f, 60.0f, 30.0f, 18000.0f, 40.0f, 15000.0f,
        0.2f, 0.0f, 0.2f,
        0, 0.25f, 1.0f, 1.0f, 0.0f, 0.3f,
        0.25f, 20.0f, 0.0f, 0.0f, -20.0f, 2.0f, 10.0f, 100.0f);

    addPreset("Capitol Studio A", "02_Studios", utf8(u8"LA。可変ルーバー。"),
        12.0f, 14.0f, 6.0f, 3, 6, 2, 0, 0.35f,
        0.1f, 0.1f, 0.20f,
        22.0f, 40.0f, 30.0f, 18000.0f, 40.0f, 16000.0f,
        0.2f, 0.0f, 0.2f,
        0, 0.25f, 1.0f, 1.0f, 0.0f, 0.3f,
        0.25f, 15.0f, 0.0f, 0.0f, -20.0f, 2.0f, 10.0f, 100.0f);

    addPreset("Blackbird St C", "02_Studios", utf8(u8"ナッシュビル。"),
        10.0f, 12.0f, 5.0f, 2, 2, 2, 2, 0.30f,
        0.0f, 0.0f, 0.20f,
        22.0f, 55.0f, 30.0f, 20000.0f, 30.0f, 18000.0f,
        0.2f, 0.0f, 0.2f,
        0, 0.30f, 1.0f, 1.0f, 0.0f, 0.3f,
        0.25f, 10.0f, 0.0f, 0.0f, -20.0f, 2.0f, 10.0f, 100.0f);

    addPreset("Ocean Way Nash A", "02_Studios", utf8(u8"教会改築スタジオ。"),
        12.0f, 18.0f, 9.0f, 3, 1, 7, 7, 0.30f,
        0.2f, 0.1f, 0.20f,
        22.0f, 55.0f, 30.0f, 18000.0f, 40.0f, 15000.0f,
        0.3f, 0.0f, 0.2f,
        0, 0.30f, 1.0f, 1.0f, 0.0f, 0.3f,
        0.30f, 25.0f, 0.0f, 0.0f, -20.0f, 2.0f, 10.0f, 100.0f);

    addPreset("Hansa Studio", "02_Studios", utf8(u8"ベルリン。ホール。"),
        15.0f, 20.0f, 8.0f, 2, 2, 2, 2, 0.35f,
        0.1f, 0.1f, 0.20f,
        15.0f, 60.0f, 30.0f, 18000.0f, 40.0f, 15000.0f,
        0.2f, 0.0f, 0.2f,
        0, 0.30f, 1.0f, 1.0f, 0.0f, 0.3f,
        0.30f, 30.0f, 0.0f, 0.0f, -20.0f, 2.0f, 10.0f, 100.0f);

    addPreset("Sound City", "02_Studios", utf8(u8"LA。ドラムサウンド。"),
        12.0f, 15.0f, 6.0f, 3, 6, 2, 2, 0.35f,
        0.0f, 0.0f, 0.20f,
        22.0f, 40.0f, 40.0f, 18000.0f, 50.0f, 16000.0f,
        0.2f, 0.0f, 0.1f,
        0, 0.25f, 1.0f, 1.0f, 0.1f, 0.3f,
        0.25f, 15.0f, 0.0f, 0.0f, -20.0f, 2.0f, 10.0f, 100.0f);

    addPreset("Power Station", "02_Studios", utf8(u8"NY。ゲートリバーブ。"),
        8.0f, 8.0f, 10.0f, 2, 2, 2, 2, 0.30f,
        0.0f, 0.0f, 0.20f,
        21.0f, 50.0f, 40.0f, 18000.0f, 50.0f, 16000.0f,
        0.2f, 0.0f, 0.3f,
        4, 0.25f, 1.0f, 1.0f, 0.0f, 0.3f,
        0.25f, 10.0f, 0.0f, 0.0f, -20.0f, 2.0f, 10.0f, 100.0f);

    addPreset("Motown Hitsville", "02_Studios", utf8(u8"デトロイト。屋根裏。"),
        4.0f, 6.0f, 2.5f, 2, 2, 6, 6, 0.25f,
        0.0f, 0.0f, 0.20f,
        20.0f, 50.0f, 100.0f, 12000.0f, 150.0f, 10000.0f,
        0.1f, 0.0f, 0.2f,
        0, 0.30f, 1.0f, 1.0f, 0.1f, 0.4f,
        0.30f, 5.0f, 0.0f, 0.0f, -20.0f, 2.0f, 10.0f, 100.0f);

    addPreset("Capitol Chamber", "02_Studios", utf8(u8"コンクリートチェンバー。"),
        5.0f, 8.0f, 3.0f, 0, 0, 0, 0, 0.20f,
        0.0f, 0.0f, 0.20f,
        18.0f, 60.0f, 80.0f, 12000.0f, 100.0f, 10000.0f,
        0.1f, 0.0f, 0.2f,
        0, 0.40f, 1.0f, 1.0f, 0.1f, 0.4f,
        0.40f, 10.0f, 0.0f, 0.0f, -20.0f, 2.0f, 10.0f, 100.0f);

    addPreset("Sun Studio", "02_Studios", utf8(u8"メンフィス。スラップ。"),
        6.0f, 8.0f, 3.5f, 3, 6, 6, 6, 0.30f,
        0.0f, 0.0f, 0.15f,
        25.0f, 60.0f, 60.0f, 16000.0f, 80.0f, 14000.0f,
        0.2f, 0.0f, 0.1f,
        0, 0.15f, 1.0f, 1.0f, 0.1f, 0.3f,
        0.15f, 40.0f, 0.0f, 0.0f, -20.0f, 2.0f, 10.0f, 100.0f);

    addPreset("RCA Studio B", "02_Studios", utf8(u8"ナッシュビル。カントリー。"),
        10.0f, 14.0f, 5.0f, 3, 6, 6, 6, 0.50f,
        0.0f, 0.0f, 0.25f,
        22.0f, 55.0f, 40.0f, 16000.0f, 50.0f, 14000.0f,
        0.3f, 0.0f, 0.3f,
        0, 0.25f, 1.0f, 1.0f, 0.0f, 0.4f,
        0.25f, 15.0f, 0.0f, 0.0f, -20.0f, 2.0f, 10.0f, 100.0f);

    addPreset("Bunkamura A", "02_Studios", utf8(u8"東京。透明感。"),
        14.0f, 18.0f, 7.0f, 3, 6, 2, 2, 0.50f,
        0.1f, 0.0f, 0.25f,
        23.0f, 50.0f, 30.0f, 20000.0f, 30.0f, 18000.0f,
        0.3f, 0.0f, 0.3f,
        0, 0.30f, 1.0f, 1.0f, 0.0f, 0.4f,
        0.25f, 20.0f, 0.0f, 0.0f, -20.0f, 2.0f, 10.0f, 100.0f);

    // ==============================================================================
    // 4. 03_Halls
    // ==============================================================================
    addPreset("Musikverein", "03_Halls", utf8(u8"ウィーン。黄金のホール。"),
        19.0f, 49.0f, 18.0f, 3, 2, 7, 7, 0.25f,
        0.2f, 0.1f, 0.20f,
        20.0f, 40.0f, 20.0f, 18000.0f, 30.0f, 16000.0f,
        0.3f, 0.0f, 0.1f,
        0, 0.35f, 1.0f, 1.0f, 0.0f, 0.3f,
        0.35f, 40.0f, 0.0f, 0.0f, -20.0f, 2.0f, 10.0f, 100.0f);

    addPreset("Concertgebouw", "03_Halls", utf8(u8"アムステルダム。"),
        28.0f, 44.0f, 17.0f, 3, 2, 7, 2, 0.25f,
        0.2f, 0.1f, 0.20f,
        20.0f, 60.0f, 20.0f, 17000.0f, 30.0f, 15000.0f,
        0.3f, 0.0f, 0.1f,
        0, 0.35f, 1.0f, 1.0f, 0.0f, 0.3f,
        0.35f, 50.0f, 0.0f, 0.0f, -20.0f, 2.0f, 10.0f, 100.0f);

    addPreset("Boston Symphony", "03_Halls", utf8(u8"ボストン。明瞭。"),
        23.0f, 38.0f, 19.0f, 3, 2, 7, 7, 0.30f,
        0.2f, 0.1f, 0.20f,
        21.0f, 50.0f, 20.0f, 20000.0f, 30.0f, 17000.0f,
        0.3f, 0.0f, 0.1f,
        0, 0.30f, 1.0f, 1.0f, 0.0f, 0.3f,
        0.30f, 35.0f, 0.0f, 0.0f, -20.0f, 2.0f, 10.0f, 100.0f);

    addPreset("Berlin Phil", "03_Halls", utf8(u8"ベルリン。ヴィンヤード。"),
        45.0f, 50.0f, 22.0f, 3, 15, 2, 2, 0.30f,
        0.2f, 0.1f, 0.20f,
        22.0f, 50.0f, 20.0f, 20000.0f, 30.0f, 18000.0f,
        0.3f, 0.0f, 0.2f,
        2, 0.30f, 1.0f, 1.0f, 0.0f, 0.3f,
        0.30f, 30.0f, 0.0f, 0.0f, -20.0f, 2.0f, 10.0f, 100.0f);

    addPreset("Suntory Hall", "03_Halls", utf8(u8"東京。木材の響き。"),
        40.0f, 45.0f, 20.0f, 3, 2, 2, 2, 0.30f,
        0.2f, 0.1f, 0.20f,
        23.0f, 55.0f, 20.0f, 20000.0f, 30.0f, 18000.0f,
        0.3f, 0.0f, 0.2f,
        2, 0.35f, 1.0f, 1.0f, 0.0f, 0.3f,
        0.35f, 35.0f, 0.0f, 0.0f, -20.0f, 2.0f, 10.0f, 100.0f);

    addPreset("Sydney Opera", "03_Halls", utf8(u8"シドニー。特徴的。"),
        30.0f, 50.0f, 20.0f, 3, 2, 2, 2, 0.30f,
        0.2f, 0.1f, 0.20f,
        22.0f, 60.0f, 20.0f, 18000.0f, 30.0f, 16000.0f,
        0.3f, 0.0f, 0.2f,
        2, 0.30f, 1.0f, 1.0f, 0.0f, 0.3f,
        0.30f, 40.0f, 0.0f, 0.0f, -20.0f, 2.0f, 10.0f, 100.0f);

    addPreset("Elbphilharmonie", "03_Halls", utf8(u8"ハンブルク。現代的。"),
        40.0f, 45.0f, 25.0f, 3, 15, 15, 15, 0.30f,
        0.2f, 0.1f, 0.20f,
        20.0f, 50.0f, 20.0f, 20000.0f, 30.0f, 18000.0f,
        0.3f, 0.0f, 0.2f,
        2, 0.30f, 1.0f, 1.0f, 0.0f, 0.3f,
        0.30f, 30.0f, 0.0f, 0.0f, -20.0f, 2.0f, 10.0f, 100.0f);

    addPreset("Walt Disney Hall", "03_Halls", utf8(u8"LA。明るく開放的。"),
        35.0f, 40.0f, 20.0f, 3, 2, 2, 2, 0.30f,
        0.2f, 0.1f, 0.20f,
        22.0f, 40.0f, 20.0f, 20000.0f, 30.0f, 18000.0f,
        0.3f, 0.0f, 0.2f,
        2, 0.30f, 1.0f, 1.0f, 0.0f, 0.3f,
        0.30f, 35.0f, 0.0f, 0.0f, -20.0f, 2.0f, 10.0f, 100.0f);

    addPreset("KKL Luzern", "03_Halls", utf8(u8"ルツェルン。静寂。"),
        25.0f, 40.0f, 20.0f, 3, 2, 15, 15, 0.30f,
        0.2f, 0.1f, 0.20f,
        18.0f, 50.0f, 20.0f, 20000.0f, 30.0f, 18000.0f,
        0.3f, 0.0f, 0.2f,
        0, 0.35f, 1.0f, 1.0f, 0.0f, 0.3f,
        0.35f, 40.0f, 0.0f, 0.0f, -20.0f, 2.0f, 10.0f, 100.0f);

    addPreset("Sapporo Kitara", "03_Halls", utf8(u8"札幌。大理石と木。"),
        30.0f, 45.0f, 20.0f, 3, 2, 11, 11, 0.30f,
        0.2f, 0.1f, 0.20f,
        20.0f, 50.0f, 20.0f, 18000.0f, 30.0f, 16000.0f,
        0.3f, 0.0f, 0.2f,
        2, 0.30f, 1.0f, 1.0f, 0.0f, 0.3f,
        0.30f, 40.0f, 0.0f, 0.0f, -20.0f, 2.0f, 10.0f, 100.0f);

    addPreset("NHK Hall", "03_Halls", utf8(u8"東京。多目的ホール。"),
        40.0f, 50.0f, 15.0f, 3, 6, 2, 2, 0.40f,
        0.1f, 0.1f, 0.20f,
        23.0f, 50.0f, 30.0f, 16000.0f, 40.0f, 14000.0f,
        0.3f, 0.0f, 0.2f,
        2, 0.30f, 1.0f, 1.0f, 0.0f, 0.3f,
        0.30f, 30.0f, 0.0f, 0.0f, -20.0f, 2.0f, 10.0f, 100.0f);

    addPreset("Tokyo Opera City", "03_Halls", utf8(u8"東京。ピラミッド天井。"),
        20.0f, 35.0f, 27.0f, 3, 2, 2, 2, 0.30f,
        0.2f, 0.1f, 0.20f,
        22.0f, 50.0f, 20.0f, 20000.0f, 30.0f, 18000.0f,
        0.3f, 0.0f, 0.3f,
        0, 0.30f, 1.0f, 1.0f, 0.0f, 0.3f,
        0.30f, 40.0f, 0.0f, 0.0f, -20.0f, 2.0f, 10.0f, 100.0f);

    addPreset("Muza Kawasaki", "03_Halls", utf8(u8"川崎。スパイラル。"),
        45.0f, 45.0f, 25.0f, 11, 1, 1, 1, 0.30f,
        0.3f, 0.1f, 0.20f,
        22.0f, 55.0f, 20.0f, 20000.0f, 30.0f, 16000.0f,
        0.3f, 0.0f, 0.2f,
        2, 0.30f, 1.0f, 1.0f, 0.0f, 0.3f,
        0.30f, 40.0f, 0.0f, 0.0f, -20.0f, 2.0f, 10.0f, 100.0f);

    addPreset("Minato Mirai", "03_Halls", utf8(u8"横浜。海の見えるホール。"),
        30.0f, 40.0f, 18.0f, 3, 1, 11, 11, 0.30f,
        0.2f, 0.1f, 0.20f,
        22.0f, 60.0f, 20.0f, 20000.0f, 30.0f, 18000.0f,
        0.3f, 0.0f, 0.2f,
        0, 0.30f, 1.0f, 1.0f, 0.0f, 0.3f,
        0.30f, 35.0f, 0.0f, 0.0f, -20.0f, 2.0f, 10.0f, 100.0f);

    addPreset("Sumida Triphony", "03_Halls", utf8(u8"東京。パイプオルガン。"),
        25.0f, 38.0f, 20.0f, 3, 1, 2, 2, 0.30f,
        0.2f, 0.1f, 0.20f,
        22.0f, 50.0f, 20.0f, 20000.0f, 30.0f, 18000.0f,
        0.3f, 0.0f, 0.2f,
        0, 0.30f, 1.0f, 1.0f, 0.0f, 0.3f,
        0.30f, 35.0f, 0.0f, 0.0f, -20.0f, 2.0f, 10.0f, 100.0f);

    addPreset("Festival Hall", "03_Halls", utf8(u8"大阪。残響の良さで有名。"),
        35.0f, 45.0f, 18.0f, 3, 2, 2, 2, 0.30f,
        0.2f, 0.1f, 0.20f,
        23.0f, 55.0f, 20.0f, 18000.0f, 30.0f, 16000.0f,
        0.3f, 0.0f, 0.2f,
        2, 0.30f, 1.0f, 1.0f, 0.0f, 0.3f,
        0.30f, 35.0f, 0.0f, 0.0f, -20.0f, 2.0f, 10.0f, 100.0f);

    addPreset("Aichi Arts", "03_Halls", utf8(u8"名古屋。大規模ホール。"),
        30.0f, 40.0f, 20.0f, 3, 2, 2, 2, 0.30f,
        0.2f, 0.1f, 0.20f,
        22.0f, 50.0f, 20.0f, 18000.0f, 30.0f, 16000.0f,
        0.3f, 0.0f, 0.2f,
        2, 0.30f, 1.0f, 1.0f, 0.0f, 0.3f,
        0.30f, 35.0f, 0.0f, 0.0f, -20.0f, 2.0f, 10.0f, 100.0f);

    // ==============================================================================
    // 5. 04_Architecture
    // ==============================================================================
    addPreset("Taj Mahal", "04_Architecture", utf8(u8"インド。総大理石。"),
        56.0f, 56.0f, 73.0f, 11, 11, 11, 11, 0.20f,
        0.1f, 0.05f, 0.20f,
        30.0f, 40.0f, 20.0f, 15000.0f, 30.0f, 10000.0f,
        0.3f, 0.0f, 0.3f,
        1, 0.70f, 1.0f, 1.0f, 0.0f, 0.3f,
        0.70f, 50.0f, 0.0f, 0.0f, -20.0f, 2.0f, 10.0f, 100.0f);

    addPreset("St. Paul's", "04_Architecture", utf8(u8"ロンドン。巨大ドーム。"),
        75.0f, 150.0f, 111.0f, 11, 11, 7, 7, 0.20f,
        0.3f, 0.1f, 0.20f,
        15.0f, 70.0f, 20.0f, 10000.0f, 30.0f, 8000.0f,
        0.3f, 0.0f, 0.3f,
        1, 0.70f, 1.0f, 1.0f, 0.0f, 0.3f,
        0.70f, 50.0f, 0.0f, 0.0f, -20.0f, 2.0f, 10.0f, 100.0f);

    addPreset("Notre Dame", "04_Architecture", utf8(u8"パリ。石造り。"),
        48.0f, 128.0f, 33.0f, 11, 7, 7, 7, 0.25f,
        0.2f, 0.1f, 0.20f,
        15.0f, 60.0f, 20.0f, 12000.0f, 30.0f, 8000.0f,
        0.3f, 0.0f, 0.3f,
        0, 0.60f, 1.0f, 1.0f, 0.0f, 0.3f,
        0.60f, 50.0f, 0.0f, 0.0f, -20.0f, 2.0f, 10.0f, 100.0f);

    addPreset("Pantheon", "04_Architecture", utf8(u8"ローマ。古代ドーム。"),
        43.0f, 43.0f, 43.0f, 11, 0, 0, 0, 0.20f,
        0.1f, 0.05f, 0.20f,
        20.0f, 50.0f, 20.0f, 14000.0f, 30.0f, 10000.0f,
        0.3f, 0.0f, 0.3f,
        1, 0.60f, 1.0f, 1.0f, 0.0f, 0.3f,
        0.60f, 40.0f, 0.0f, 0.0f, -20.0f, 2.0f, 10.0f, 100.0f);

    addPreset("Hagia Sophia", "04_Architecture", utf8(u8"イスタンブール。巨大。"),
        70.0f, 80.0f, 55.0f, 11, 11, 7, 7, 0.20f,
        0.2f, 0.1f, 0.20f,
        20.0f, 60.0f, 20.0f, 12000.0f, 30.0f, 9000.0f,
        0.3f, 0.0f, 0.3f,
        1, 0.70f, 1.0f, 1.0f, 0.0f, 0.3f,
        0.70f, 50.0f, 0.0f, 0.0f, -20.0f, 2.0f, 10.0f, 100.0f);

    addPreset("Cologne Cathedral", "04_Architecture", utf8(u8"ドイツ。天を突く。"),
        45.0f, 144.0f, 43.0f, 11, 7, 7, 7, 0.20f,
        0.2f, 0.1f, 0.20f,
        15.0f, 65.0f, 20.0f, 12000.0f, 30.0f, 9000.0f,
        0.3f, 0.0f, 0.3f,
        0, 0.60f, 1.0f, 1.0f, 0.0f, 0.3f,
        0.60f, 50.0f, 0.0f, 0.0f, -20.0f, 2.0f, 10.0f, 100.0f);

    addPreset("St. Peter's", "04_Architecture", utf8(u8"バチカン。世界最大級。"),
        100.0f, 200.0f, 130.0f, 11, 11, 11, 11, 0.20f,
        0.3f, 0.1f, 0.20f,
        20.0f, 55.0f, 20.0f, 10000.0f, 30.0f, 8000.0f,
        0.3f, 0.0f, 0.3f,
        1, 0.70f, 1.0f, 1.0f, 0.0f, 0.3f,
        0.70f, 50.0f, 0.0f, 0.0f, -20.0f, 2.0f, 10.0f, 100.0f);

    addPreset("Sagrada Familia", "04_Architecture", utf8(u8"バルセロナ。石の森。"),
        60.0f, 90.0f, 170.0f, 11, 7, 7, 7, 0.20f,
        0.2f, 0.1f, 0.20f,
        20.0f, 60.0f, 20.0f, 14000.0f, 30.0f, 10000.0f,
        0.3f, 0.0f, 0.3f,
        6, 0.60f, 1.0f, 1.0f, 0.0f, 0.3f,
        0.60f, 50.0f, 0.0f, 0.0f, -20.0f, 2.0f, 10.0f, 100.0f);

    addPreset("Pyramids", "04_Architecture", utf8(u8"エジプト。王の間。"),
        10.0f, 20.0f, 6.0f, 11, 11, 11, 11, 0.25f,
        0.0f, 0.0f, 0.20f,
        35.0f, 20.0f, 50.0f, 10000.0f, 50.0f, 8000.0f,
        0.2f, 0.0f, 0.3f,
        0, 0.40f, 1.0f, 1.0f, 0.0f, 0.3f,
        0.40f, 30.0f, 0.0f, 0.0f, -20.0f, 2.0f, 10.0f, 100.0f);

    addPreset("Hamilton Mausoleum", "04_Architecture", utf8(u8"スコットランド。"),
        10.0f, 10.0f, 30.0f, 11, 11, 11, 11, 0.15f,
        0.1f, 0.05f, 0.25f,
        10.0f, 80.0f, 20.0f, 8000.0f, 20.0f, 6000.0f,
        0.2f, 0.0f, 0.3f,
        1, 0.60f, 1.0f, 1.0f, 0.0f, 0.3f,
        0.70f, 20.0f, 0.0f, 0.0f, -20.0f, 2.0f, 10.0f, 100.0f);

    addPreset("Gol Gumbaz", "04_Architecture", utf8(u8"インド。ささやき。"),
        40.0f, 40.0f, 50.0f, 11, 11, 11, 11, 0.20f,
        0.1f, 0.05f, 0.20f,
        30.0f, 40.0f, 20.0f, 12000.0f, 30.0f, 10000.0f,
        0.3f, 0.0f, 0.3f,
        1, 0.60f, 1.0f, 1.0f, 0.0f, 0.3f,
        0.60f, 30.0f, 0.0f, 0.0f, -20.0f, 2.0f, 10.0f, 100.0f);

    addPreset("Teufelsberg", "04_Architecture", utf8(u8"ベルリン。盗聴ドーム。"),
        20.0f, 20.0f, 20.0f, 0, 15, 15, 15, 0.25f,
        0.2f, 0.1f, 0.20f,
        15.0f, 60.0f, 20.0f, 15000.0f, 30.0f, 12000.0f,
        0.3f, 0.0f, 0.3f,
        1, 0.50f, 1.0f, 1.0f, 0.1f, 0.3f,
        0.50f, 20.0f, 0.0f, 0.0f, -20.0f, 2.0f, 10.0f, 100.0f);

    addPreset("Inchindown Tanks", "04_Architecture", utf8(u8"スコットランド。世界記録。"),
        10.0f, 240.0f, 10.0f, 0, 0, 0, 0, 0.10f,
        0.1f, 0.05f, 0.25f,
        8.0f, 90.0f, 20.0f, 5000.0f, 20.0f, 4000.0f,
        0.3f, 0.0f, 0.3f,
        3, 0.60f, 1.0f, 1.0f, 0.0f, 0.3f,
        0.80f, 30.0f, 0.0f, 0.0f, -20.0f, 2.0f, 10.0f, 100.0f);

    addPreset("Cistern", "04_Architecture", utf8(u8"アメリカ。古貯水槽。"),
        30.0f, 30.0f, 10.0f, 0, 0, 0, 0, 0.15f,
        0.1f, 0.05f, 0.25f,
        15.0f, 80.0f, 20.0f, 6000.0f, 20.0f, 5000.0f,
        0.3f, 0.0f, 0.3f,
        3, 0.60f, 1.0f, 1.0f, 0.0f, 0.3f,
        0.80f, 30.0f, 0.0f, 0.0f, -20.0f, 2.0f, 10.0f, 100.0f);

    addPreset("Cooling Tower", "04_Architecture", utf8(u8"冷却塔。"),
        80.0f, 80.0f, 150.0f, 0, 12, 0, 0, 0.20f,
        0.2f, 0.1f, 0.20f,
        40.0f, 60.0f, 20.0f, 10000.0f, 30.0f, 8000.0f,
        0.3f, 0.0f, 0.3f,
        3, 0.50f, 1.0f, 1.0f, 0.0f, 0.3f,
        0.60f, 40.0f, 0.0f, 0.0f, -20.0f, 2.0f, 10.0f, 100.0f);

    addPreset("Tunnel", "04_Architecture", utf8(u8"長いトンネル。"),
        8.0f, 500.0f, 6.0f, 0, 0, 0, 0, 0.15f,
        0.1f, 0.05f, 0.20f,
        15.0f, 70.0f, 20.0f, 8000.0f, 30.0f, 6000.0f,
        0.3f, 0.0f, 0.3f,
        3, 0.50f, 1.0f, 1.0f, 0.0f, 0.3f,
        0.70f, 30.0f, 0.0f, 0.0f, -20.0f, 2.0f, 10.0f, 100.0f);

    addPreset("Subway Station", "04_Architecture", utf8(u8"地下鉄駅。"),
        20.0f, 100.0f, 6.0f, 11, 0, 11, 11, 0.20f,
        0.2f, 0.1f, 0.20f,
        20.0f, 60.0f, 20.0f, 12000.0f, 30.0f, 10000.0f,
        0.3f, 0.0f, 0.3f,
        0, 0.40f, 1.0f, 1.0f, 0.1f, 0.3f,
        0.50f, 20.0f, 0.0f, 0.0f, -20.0f, 2.0f, 10.0f, 100.0f);

    addPreset("Swimming Pool", "04_Architecture", utf8(u8"屋内プール。"),
        25.0f, 50.0f, 10.0f, 10, 15, 11, 8, 0.25f,
        0.5f, 0.2f, 0.20f,
        28.0f, 90.0f, 20.0f, 10000.0f, 30.0f, 8000.0f,
        0.3f, 0.0f, 0.3f,
        0, 0.40f, 1.0f, 1.0f, 0.0f, 0.3f,
        0.50f, 40.0f, 0.0f, 0.0f, -20.0f, 2.0f, 10.0f, 100.0f);

    addPreset("Gymnasium", "04_Architecture", utf8(u8"体育館。"),
        30.0f, 40.0f, 12.0f, 2, 9, 2, 2, 0.30f,
        0.1f, 0.05f, 0.20f,
        20.0f, 50.0f, 20.0f, 14000.0f, 30.0f, 12000.0f,
        0.3f, 0.0f, 0.3f,
        0, 0.40f, 1.0f, 1.0f, 0.0f, 0.3f,
        0.40f, 30.0f, 0.0f, 0.0f, -20.0f, 2.0f, 10.0f, 100.0f);

    addPreset("Warehouse", "04_Architecture", utf8(u8"巨大倉庫。"),
        50.0f, 80.0f, 15.0f, 0, 9, 9, 9, 0.25f,
        0.1f, 0.05f, 0.20f,
        15.0f, 50.0f, 20.0f, 12000.0f, 30.0f, 10000.0f,
        0.3f, 0.0f, 0.3f,
        0, 0.40f, 1.0f, 1.0f, 0.0f, 0.3f,
        0.50f, 30.0f, 0.0f, 0.0f, -20.0f, 2.0f, 10.0f, 100.0f);

    // ==============================================================================
    // 6. 05_Vintage
    // ==============================================================================
    addPreset("EMT 140 Bright", "05_Vintage", utf8(u8"鉄板リバーブ。"),
        2.0f, 3.0f, 0.1f, 9, 9, 9, 9, 0.20f,
        0.0f, 0.0f, 0.20f,
        20.0f, 50.0f, 100.0f, 20000.0f, 100.0f, 18000.0f,
        0.1f, 0.0f, 0.1f,
        0, 0.30f, 1.0f, 1.0f, 0.4f, 0.5f,
        0.30f, 0.0f, 0.0f, 0.0f, -20.0f, 2.0f, 10.0f, 100.0f);

    addPreset("EMT 140 Dark", "05_Vintage", utf8(u8"鉄板リバーブ。"),
        2.0f, 3.0f, 0.1f, 9, 9, 9, 9, 0.35f,
        0.0f, 0.0f, 0.20f,
        20.0f, 50.0f, 100.0f, 8000.0f, 100.0f, 6000.0f,
        0.1f, 0.0f, 0.1f,
        0, 0.30f, 1.0f, 1.0f, 0.4f, 0.5f,
        0.20f, 0.0f, 0.0f, 0.0f, -20.0f, 2.0f, 10.0f, 100.0f);

    addPreset("EMT 250", "05_Vintage", utf8(u8"初期デジタル。"),
        10.0f, 10.0f, 5.0f, 0, 0, 0, 0, 0.30f,
        0.5f, 0.2f, 0.20f,
        20.0f, 50.0f, 20.0f, 10000.0f, 20.0f, 10000.0f,
        0.2f, 0.0f, 0.2f,
        6, 0.20f, 1.0f, 1.0f, 0.2f, 0.4f,
        0.30f, 20.0f, 0.0f, 0.0f, -20.0f, 2.0f, 10.0f, 100.0f);

    addPreset("AMS RMX16 NonLin", "05_Vintage", utf8(u8"ゲートリバーブ。"),
        5.0f, 5.0f, 3.0f, 0, 0, 0, 0, 0.40f,
        0.0f, 0.0f, 0.30f,
        20.0f, 50.0f, 50.0f, 12000.0f, 50.0f, 12000.0f,
        0.1f, 0.0f, 0.3f,
        4, 0.20f, 1.0f, 1.0f, 0.3f, 0.5f,
        0.20f, 0.0f, 0.0f, 0.0f, -20.0f, 2.0f, 10.0f, 100.0f);

    addPreset("AMS RMX16 Amb", "05_Vintage", utf8(u8"短いアンビエンス。"),
        3.0f, 3.0f, 2.0f, 0, 0, 0, 0, 0.50f,
        0.0f, 0.0f, 0.20f,
        20.0f, 50.0f, 50.0f, 14000.0f, 50.0f, 14000.0f,
        0.2f, 0.0f, 0.3f,
        4, 0.20f, 1.0f, 1.0f, 0.2f, 0.5f,
        0.15f, 0.0f, 0.0f, 0.0f, -20.0f, 2.0f, 10.0f, 100.0f);

    addPreset("Lexicon 224 Hall", "05_Vintage", utf8(u8"80年代の広大なホール。"),
        20.0f, 20.0f, 10.0f, 0, 0, 0, 0, 0.30f,
        0.8f, 0.4f, 0.25f,
        20.0f, 50.0f, 20.0f, 8000.0f, 20.0f, 6000.0f,
        0.2f, 0.0f, 0.2f,
        6, 0.30f, 1.0f, 1.0f, 0.1f, 0.4f,
        0.40f, 40.0f, 0.0f, 0.0f, -20.0f, 2.0f, 10.0f, 100.0f);

    addPreset("Lexicon 480L Wood", "05_Vintage", utf8(u8"木造ルーム。"),
        8.0f, 10.0f, 4.0f, 2, 2, 2, 2, 0.35f,
        0.5f, 0.2f, 0.20f,
        20.0f, 50.0f, 50.0f, 12000.0f, 50.0f, 10000.0f,
        0.2f, 0.0f, 0.3f,
        6, 0.30f, 1.0f, 1.0f, 0.0f, 0.4f,
        0.30f, 10.0f, 0.0f, 0.0f, -20.0f, 2.0f, 10.0f, 100.0f);

    addPreset("Spring Reverb", "05_Vintage", utf8(u8"スプリング。"),
        1.0f, 2.0f, 0.5f, 9, 9, 9, 9, 0.25f,
        0.0f, 0.0f, 0.20f,
        20.0f, 50.0f, 150.0f, 6000.0f, 150.0f, 5000.0f,
        0.1f, 0.0f, 0.1f,
        3, 0.20f, 1.0f, 1.0f, 0.4f, 0.5f,
        0.25f, 0.0f, 0.0f, 0.0f, -20.0f, 2.0f, 10.0f, 100.0f);

    addPreset("AKG BX20", "05_Vintage", utf8(u8"スプリングタワー。"),
        2.0f, 2.0f, 2.0f, 9, 9, 9, 9, 0.25f,
        0.0f, 0.0f, 0.20f,
        20.0f, 50.0f, 100.0f, 8000.0f, 100.0f, 7000.0f,
        0.2f, 0.0f, 0.2f,
        3, 0.30f, 1.0f, 1.0f, 0.2f, 0.5f,
        0.30f, 5.0f, 0.0f, 0.0f, -20.0f, 2.0f, 10.0f, 100.0f);

    addPreset("Quantec QRS", "05_Vintage", utf8(u8"リアルな初期デジタル。"),
        15.0f, 20.0f, 8.0f, 0, 0, 0, 0, 0.30f,
        0.0f, 0.0f, 0.20f,
        20.0f, 50.0f, 20.0f, 16000.0f, 20.0f, 16000.0f,
        0.2f, 0.0f, 0.3f,
        0, 0.30f, 1.0f, 1.0f, 0.1f, 0.4f,
        0.30f, 10.0f, 0.0f, 0.0f, -20.0f, 2.0f, 10.0f, 100.0f);

    addPreset("Eventide Blackhole", "05_Vintage", utf8(u8"宇宙的な広がり。"),
        50.0f, 50.0f, 50.0f, 12, 12, 12, 12, 0.05f,
        1.0f, 0.8f, 0.30f,
        20.0f, 50.0f, 20.0f, 20000.0f, 20.0f, 20000.0f,
        0.3f, 0.0f, 0.2f,
        6, 0.50f, 1.5f, 1.0f, 0.2f, 0.5f,
        0.80f, 50.0f, 0.0f, 0.0f, -20.0f, 2.0f, 10.0f, 100.0f);

    addPreset("Ursa Major", "05_Vintage", utf8(u8"マルチタップ風。"),
        10.0f, 10.0f, 5.0f, 0, 0, 0, 0, 0.35f,
        0.5f, 0.5f, 0.20f,
        20.0f, 50.0f, 50.0f, 10000.0f, 50.0f, 8000.0f,
        0.2f, 0.0f, 0.3f,
        4, 0.20f, 1.0f, 1.0f, 0.2f, 0.5f,
        0.30f, 0.0f, 0.0f, 0.0f, -20.0f, 2.0f, 10.0f, 100.0f);

    addPreset("Yamaha REV7", "05_Vintage", utf8(u8"80年代標準機。"),
        12.0f, 15.0f, 6.0f, 0, 0, 0, 0, 0.35f,
        0.2f, 0.1f, 0.20f,
        20.0f, 50.0f, 40.0f, 12000.0f, 40.0f, 10000.0f,
        0.2f, 0.0f, 0.3f,
        0, 0.25f, 1.0f, 1.0f, 0.1f, 0.4f,
        0.30f, 0.0f, 0.0f, 0.0f, -20.0f, 2.0f, 10.0f, 100.0f);

    addPreset("Roland SRV-2000", "05_Vintage", utf8(u8"非線形な響き。"),
        8.0f, 10.0f, 4.0f, 0, 0, 0, 0, 0.40f,
        0.3f, 0.2f, 0.20f,
        20.0f, 50.0f, 50.0f, 14000.0f, 50.0f, 12000.0f,
        0.2f, 0.0f, 0.3f,
        6, 0.30f, 1.0f, 1.0f, 0.1f, 0.4f,
        0.30f, 0.0f, 0.0f, 0.0f, -20.0f, 2.0f, 10.0f, 100.0f);

    addPreset("Sony DRE-2000", "05_Vintage", utf8(u8"初期の畳み込み風。"),
        15.0f, 20.0f, 8.0f, 0, 0, 0, 0, 0.25f,
        0.0f, 0.0f, 0.20f,
        20.0f, 50.0f, 20.0f, 18000.0f, 20.0f, 18000.0f,
        0.2f, 0.0f, 0.3f,
        0, 0.20f, 1.0f, 1.0f, 0.0f, 0.3f,
        0.30f, 0.0f, 0.0f, 0.0f, -20.0f, 2.0f, 10.0f, 100.0f);

    // ==============================================================================
    // 7. 06_Instruments
    // ==============================================================================
    addPreset("Snare Plate", "06_Instruments", utf8(u8"スネア用。"),
        3.0f, 3.0f, 0.1f, 9, 9, 9, 9, 0.35f,
        0.0f, 0.0f, 0.15f,
        20.0f, 50.0f, 150.0f, 12000.0f, 150.0f, 10000.0f,
        0.1f, 0.0f, 0.3f,
        0, 0.25f, 1.0f, 1.0f, 0.1f, 0.5f,
        0.20f, 0.0f, 0.0f, 0.0f, -20.0f, 2.0f, 10.0f, 100.0f);

    addPreset("Kick Ambience", "06_Instruments", utf8(u8"キック用。"),
        4.0f, 5.0f, 3.0f, 2, 2, 2, 2, 0.45f,
        0.0f, 0.0f, 0.15f,
        20.0f, 50.0f, 20.0f, 5000.0f, 20.0f, 4000.0f,
        0.2f, 0.0f, 0.1f,
        0, 0.15f, 1.0f, 1.0f, 0.0f, 0.4f,
        0.15f, 0.0f, 0.0f, 0.0f, -20.0f, 2.0f, 10.0f, 100.0f);

    addPreset("Room Overhead", "06_Instruments", utf8(u8"ドラム全体。"),
        6.0f, 8.0f, 4.0f, 2, 6, 7, 7, 0.40f,
        0.0f, 0.0f, 0.20f,
        20.0f, 50.0f, 50.0f, 16000.0f, 50.0f, 14000.0f,
        0.2f, 0.0f, 0.3f,
        0, 0.30f, 1.0f, 1.0f, 0.0f, 0.4f,
        0.50f, 0.0f, 0.0f, 0.0f, -20.0f, 2.0f, 10.0f, 100.0f);

    addPreset("Tom Reso", "06_Instruments", utf8(u8"タムの余韻。"),
        5.0f, 6.0f, 3.0f, 2, 2, 2, 2, 0.40f,
        0.0f, 0.0f, 0.20f,
        20.0f, 50.0f, 80.0f, 10000.0f, 80.0f, 8000.0f,
        0.2f, 0.0f, 0.2f,
        0, 0.20f, 1.0f, 1.0f, 0.0f, 0.4f,
        0.40f, 0.0f, 0.0f, 0.0f, -20.0f, 2.0f, 10.0f, 100.0f);

    addPreset("Lead Vocal Plate", "06_Instruments", utf8(u8"ボーカル用。"),
        3.0f, 4.0f, 0.1f, 9, 9, 9, 9, 0.30f,
        0.1f, 0.1f, 0.15f,
        20.0f, 50.0f, 120.0f, 10000.0f, 120.0f, 10000.0f,
        0.1f, 0.0f, 0.3f,
        0, 0.30f, 1.0f, 1.0f, 0.0f, 0.5f,
        0.30f, 20.0f, 0.0f, 0.0f, -20.0f, 2.0f, 10.0f, 100.0f);

    addPreset("Backing Vocal Room", "06_Instruments", utf8(u8"コーラス用。"),
        5.0f, 6.0f, 3.0f, 3, 6, 2, 2, 0.40f,
        0.0f, 0.0f, 0.15f,
        20.0f, 50.0f, 100.0f, 12000.0f, 100.0f, 10000.0f,
        0.2f, 0.0f, 0.2f,
        0, 0.20f, 1.0f, 1.0f, 0.0f, 0.4f,
        0.20f, 10.0f, 0.0f, 0.0f, -20.0f, 2.0f, 10.0f, 100.0f);

    addPreset("Ballad Hall", "06_Instruments", utf8(u8"バラード用。"),
        20.0f, 30.0f, 10.0f, 3, 2, 7, 7, 0.30f,
        0.2f, 0.1f, 0.20f,
        20.0f, 50.0f, 100.0f, 10000.0f, 100.0f, 8000.0f,
        0.3f, 0.0f, 0.2f,
        0, 0.30f, 1.0f, 1.0f, 0.0f, 0.3f,
        0.40f, 50.0f, 0.0f, 0.0f, -20.0f, 2.0f, 10.0f, 100.0f);

    addPreset("Rap Booth", "06_Instruments", utf8(u8"ラップ用。"),
        1.5f, 2.0f, 2.2f, 4, 6, 6, 6, 0.50f,
        0.0f, 0.0f, 0.10f,
        20.0f, 50.0f, 100.0f, 15000.0f, 100.0f, 15000.0f,
        0.1f, 0.0f, 0.2f,
        0, 0.10f, 1.0f, 1.0f, 0.0f, 0.3f,
        0.30f, 0.0f, 0.0f, 0.0f, -20.0f, 2.0f, 10.0f, 100.0f);

    addPreset("Acoustic Room", "06_Instruments", utf8(u8"アコギ用。"),
        4.0f, 5.0f, 2.8f, 3, 2, 2, 2, 0.35f,
        0.0f, 0.0f, 0.15f,
        20.0f, 50.0f, 80.0f, 14000.0f, 80.0f, 12000.0f,
        0.2f, 0.0f, 0.2f,
        0, 0.20f, 1.0f, 1.0f, 0.0f, 0.3f,
        0.20f, 10.0f, 0.0f, 0.0f, -20.0f, 2.0f, 10.0f, 100.0f);

    addPreset("Electric Spring", "06_Instruments", utf8(u8"エレキ用。"),
        1.0f, 2.0f, 0.5f, 9, 9, 9, 9, 0.25f,
        0.0f, 0.0f, 0.25f,
        20.0f, 50.0f, 150.0f, 6000.0f, 150.0f, 5000.0f,
        0.1f, 0.0f, 0.1f,
        3, 0.25f, 1.0f, 1.0f, 0.3f, 0.5f,
        0.30f, 0.0f, 0.0f, 0.0f, -20.0f, 2.0f, 10.0f, 100.0f);

    addPreset("Flamenco Hall", "06_Instruments", utf8(u8"スパニッシュギター。"),
        10.0f, 15.0f, 6.0f, 1, 2, 7, 7, 0.30f,
        0.0f, 0.0f, 0.25f,
        25.0f, 40.0f, 80.0f, 16000.0f, 80.0f, 14000.0f,
        0.2f, 0.0f, 0.2f,
        0, 0.30f, 1.0f, 1.0f, 0.0f, 0.5f,
        0.50f, 0.0f, 0.0f, 0.0f, -20.0f, 2.0f, 10.0f, 100.0f);

    addPreset("Concert Grand", "06_Instruments", utf8(u8"グランドピアノ。"),
        25.0f, 35.0f, 15.0f, 3, 2, 2, 2, 0.25f,
        0.1f, 0.1f, 0.20f,
        20.0f, 50.0f, 20.0f, 18000.0f, 20.0f, 16000.0f,
        0.3f, 0.0f, 0.2f,
        0, 0.30f, 1.0f, 1.0f, 0.0f, 0.3f,
        0.35f, 20.0f, 0.0f, 0.0f, -20.0f, 2.0f, 10.0f, 100.0f);

    addPreset("Upright Room", "06_Instruments", utf8(u8"アップライト。"),
        4.0f, 5.0f, 2.6f, 3, 6, 2, 2, 0.35f,
        0.0f, 0.0f, 0.20f,
        20.0f, 50.0f, 50.0f, 14000.0f, 50.0f, 12000.0f,
        0.2f, 0.0f, 0.2f,
        0, 0.20f, 1.0f, 1.0f, 0.0f, 0.4f,
        0.40f, 0.0f, 0.0f, 0.0f, -20.0f, 2.0f, 10.0f, 100.0f);

    addPreset("Jazz Club Piano", "06_Instruments", utf8(u8"ジャズクラブ。"),
        8.0f, 12.0f, 3.5f, 3, 6, 5, 5, 0.40f,
        0.1f, 0.1f, 0.25f,
        25.0f, 60.0f, 40.0f, 12000.0f, 40.0f, 10000.0f,
        0.2f, 0.0f, 0.2f,
        0, 0.25f, 1.0f, 1.0f, 0.0f, 0.4f,
        0.40f, 0.0f, 0.0f, 0.0f, -20.0f, 2.0f, 10.0f, 100.0f);

    addPreset("Chamber Strings", "06_Instruments", utf8(u8"小編成弦。"),
        10.0f, 15.0f, 6.0f, 3, 2, 2, 2, 0.30f,
        0.1f, 0.1f, 0.25f,
        20.0f, 50.0f, 40.0f, 16000.0f, 40.0f, 14000.0f,
        0.2f, 0.0f, 0.2f,
        0, 0.30f, 1.0f, 1.0f, 0.0f, 0.5f,
        0.50f, 0.0f, 0.0f, 0.0f, -20.0f, 2.0f, 10.0f, 100.0f);

    addPreset("Quartet Room", "06_Instruments", utf8(u8"四重奏用。"),
        6.0f, 8.0f, 4.0f, 3, 2, 2, 2, 0.35f,
        0.0f, 0.0f, 0.25f,
        20.0f, 50.0f, 50.0f, 16000.0f, 50.0f, 14000.0f,
        0.2f, 0.0f, 0.2f,
        0, 0.25f, 1.0f, 1.0f, 0.0f, 0.4f,
        0.40f, 0.0f, 0.0f, 0.0f, -20.0f, 2.0f, 10.0f, 100.0f);

    addPreset("Symphonic Hall", "06_Instruments", utf8(u8"フルオケ用。"),
        30.0f, 50.0f, 20.0f, 3, 2, 7, 7, 0.25f,
        0.2f, 0.1f, 0.30f,
        20.0f, 50.0f, 20.0f, 18000.0f, 20.0f, 16000.0f,
        0.3f, 0.0f, 0.2f,
        0, 0.30f, 1.0f, 1.0f, 0.0f, 0.5f,
        0.40f, 0.0f, 0.0f, 0.0f, -20.0f, 2.0f, 10.0f, 100.0f);

    addPreset("Pad Wash", "06_Instruments", utf8(u8"シンセパッド。"),
        20.0f, 20.0f, 10.0f, 12, 12, 12, 12, 0.20f,
        0.8f, 0.5f, 0.40f,
        20.0f, 50.0f, 20.0f, 20000.0f, 20.0f, 20000.0f,
        0.2f, 0.0f, 0.3f,
        6, 0.50f, 1.5f, 1.0f, 0.0f, 0.6f,
        0.60f, 0.0f, 0.0f, 0.0f, -20.0f, 2.0f, 10.0f, 100.0f);

    addPreset("Lead Delay-Verb", "06_Instruments", utf8(u8"リードシンセ。"),
        15.0f, 15.0f, 5.0f, 9, 9, 9, 9, 0.30f,
        0.5f, 0.3f, 0.25f,
        20.0f, 50.0f, 100.0f, 15000.0f, 100.0f, 12000.0f,
        0.2f, 0.0f, 0.2f,
        4, 0.20f, 1.2f, 1.0f, 0.1f, 0.5f,
        0.30f, 50.0f, 0.0f, 0.0f, -20.0f, 2.0f, 10.0f, 100.0f);

    addPreset("Pluck Ambience", "06_Instruments", utf8(u8"プラック音色。"),
        5.0f, 5.0f, 3.0f, 8, 8, 8, 8, 0.35f,
        0.0f, 0.0f, 0.25f,
        20.0f, 50.0f, 50.0f, 18000.0f, 50.0f, 16000.0f,
        0.1f, 0.0f, 0.2f,
        0, 0.20f, 1.2f, 1.0f, 0.0f, 0.4f,
        0.40f, 0.0f, 0.0f, 0.0f, -20.0f, 2.0f, 10.0f, 100.0f);

    // ==============================================================================
    // 8. 07_Nature_FX
    // ==============================================================================
    addPreset("Grand Canyon", "07_Nature_FX", utf8(u8"アリゾナ。"),
        300.0f, 300.0f, 300.0f, 19, 12, 19, 19, 0.20f,
        0.0f, 0.0f, 0.40f,
        25.0f, 20.0f, 20.0f, 20000.0f, 100.0f, 15000.0f,
        0.3f, 0.0f, 0.1f,
        2, 0.30f, 2.0f, 1.0f, 0.0f, 0.0f,
        1.00f, 0.0f, 0.0f, 0.0f, -20.0f, 2.0f, 10.0f, 100.0f);

    addPreset("Deep Forest", "07_Nature_FX", utf8(u8"アマゾン。"),
        100.0f, 100.0f, 40.0f, 18, 12, 18, 18, 0.30f,
        0.2f, 0.4f, 0.30f,
        28.0f, 90.0f, 20.0f, 6000.0f, 120.0f, 5000.0f,
        0.3f, 0.0f, 0.1f,
        0, 0.50f, 1.2f, 1.0f, 0.0f, 0.5f,
        0.50f, 0.0f, 0.0f, 0.0f, -20.0f, 2.0f, 10.0f, 100.0f);

    addPreset("Ice Cave", "07_Nature_FX", utf8(u8"氷の洞窟。"),
        20.0f, 50.0f, 10.0f, 26, 26, 26, 26, 0.15f,
        0.0f, 0.0f, 0.30f,
        -5.0f, 40.0f, 20.0f, 20000.0f, 50.0f, 18000.0f,
        0.2f, 0.0f, 0.2f,
        6, 0.20f, 1.0f, 1.0f, 0.0f, 0.4f,
        0.20f, 0.0f, 0.0f, 0.0f, -20.0f, 2.0f, 10.0f, 100.0f);

    addPreset("Limestone Cave", "07_Nature_FX", utf8(u8"鍾乳洞。"),
        50.0f, 100.0f, 30.0f, 19, 19, 19, 19, 0.20f,
        0.1f, 0.1f, 0.30f,
        15.0f, 95.0f, 20.0f, 8000.0f, 30.0f, 6000.0f,
        0.3f, 0.0f, 0.2f,
        6, 0.50f, 1.0f, 1.0f, 0.0f, 0.6f,
        0.50f, 0.0f, 0.0f, 0.0f, -20.0f, 2.0f, 10.0f, 100.0f);

    addPreset("Underwater Deep", "07_Nature_FX", utf8(u8"深海。"),
        100.0f, 100.0f, 50.0f, 10, 10, 10, 10, 0.30f,
        0.2f, 0.5f, 0.50f,
        4.0f, 100.0f, 20.0f, 3000.0f, 20.0f, 2000.0f,
        0.3f, 0.0f, 0.1f,
        1, 0.50f, 0.5f, 1.0f, 0.2f, 0.8f,
        0.50f, 0.0f, 0.0f, 0.0f, -20.0f, 2.0f, 10.0f, 100.0f);

    addPreset("Outer Space", "07_Nature_FX", utf8(u8"宇宙。真空。"),
        300.0f, 300.0f, 300.0f, 12, 12, 12, 12, 0.05f,
        0.0f, 0.0f, 0.30f,
        -100.0f, 0.0f, 20.0f, 20000.0f, 20.0f, 20000.0f,
        0.3f, 0.0f, 0.1f,
        5, 0.00f, 1.0f, 1.0f, 0.0f, 0.0f,
        1.00f, 0.0f, 0.0f, 0.0f, -20.0f, 2.0f, 10.0f, 100.0f);

    addPreset("Inside a Whale", "07_Nature_FX", utf8(u8"クジラの体内。"),
        15.0f, 30.0f, 10.0f, 21, 20, 20, 20, 0.40f,
        0.1f, 0.8f, 0.35f,
        36.0f, 80.0f, 20.0f, 2000.0f, 80.0f, 1500.0f,
        0.2f, 0.0f, 0.2f,
        1, 0.40f, 0.8f, 1.0f, 0.2f, 0.5f,
        0.30f, 0.0f, 0.0f, 0.0f, -20.0f, 2.0f, 10.0f, 100.0f);

    addPreset("Reverse Swell", "07_Nature_FX", utf8(u8"逆再生風。"),
        50.0f, 50.0f, 20.0f, 11, 11, 11, 11, 0.20f,
        0.2f, 0.2f, 1.00f,
        20.0f, 50.0f, 20.0f, 20000.0f, 20.0f, 20000.0f,
        0.3f, 0.0f, 0.2f,
        2, 0.60f, 1.5f, 1.0f, 0.0f, 1.0f,
        1.00f, 500.0f, 0.0f, 0.0f, -20.0f, 2.0f, 10.0f, 100.0f);

    addPreset("Freeze Drone", "07_Nature_FX", utf8(u8"フリーズ。"),
        100.0f, 100.0f, 50.0f, 12, 12, 12, 12, 0.05f,
        0.5f, 0.5f, 1.00f,
        20.0f, 50.0f, 20.0f, 20000.0f, 20.0f, 20000.0f,
        0.3f, 0.0f, 0.2f,
        5, 0.70f, 1.0f, 1.0f, 0.5f, 0.5f,
        1.00f, 0.0f, 0.0f, 0.0f, -20.0f, 2.0f, 10.0f, 100.0f);

    addPreset("Lo-Fi Radio", "07_Nature_FX", utf8(u8"ラジオボイス。"),
        2.0f, 2.0f, 2.0f, 9, 9, 9, 9, 0.35f,
        0.0f, 0.0f, 0.40f,
        20.0f, 50.0f, 400.0f, 3000.0f, 400.0f, 3000.0f,
        0.1f, 0.0f, 0.2f,
        0, 0.00f, 1.0f, 1.0f, 0.8f, 0.5f,
        0.10f, 0.0f, 0.0f, 0.0f, -20.0f, 2.0f, 10.0f, 100.0f);

    addPreset("Bloom", "07_Nature_FX", utf8(u8"ブルーム。"),
        30.0f, 30.0f, 10.0f, 11, 11, 11, 11, 0.25f,
        0.2f, 0.3f, 0.50f,
        20.0f, 50.0f, 20.0f, 10000.0f, 20.0f, 8000.0f,
        0.3f, 0.0f, 0.2f,
        6, 0.60f, 1.0f, 1.0f, 0.0f, 0.5f,
        0.60f, 100.0f, 0.0f, 0.0f, -20.0f, 2.0f, 10.0f, 100.0f);

    addPreset("Shimmer Sim", "07_Nature_FX", utf8(u8"擬似シマー。"),
        20.0f, 20.0f, 10.0f, 8, 8, 8, 8, 0.30f,
        2.0f, 1.0f, 0.40f,
        20.0f, 50.0f, 20.0f, 20000.0f, 20.0f, 20000.0f,
        0.3f, 0.0f, 0.2f,
        5, 0.50f, 1.5f, 1.0f, 0.0f, 0.5f,
        0.50f, 0.0f, 0.0f, 0.0f, -20.0f, 2.0f, 10.0f, 100.0f);

    addPreset("Black Hole", "04_SciFi", utf8(u8"事象の地平面。"),
        200.0f, 200.0f, 200.0f, 12, 12, 12, 12, 0.05f,
        0.1f, 0.2f, 1.00f,
        -270.0f, 0.0f, 20.0f, 20000.0f, 20.0f, 5000.0f,
        0.3f, 0.0f, 0.1f,
        1, 0.00f, 1.0f, 1.0f, 0.5f, 0.5f,
        1.00f, 100.0f, 0.0f, 0.0f, -20.0f, 2.0f, 10.0f, 100.0f);
}
// ==============================================================================
// 3. STATE MANAGEMENT & INFO
// ==============================================================================

const juce::String FdnReverbAudioProcessor::getName() const { return "FDN Physics Reverb"; }
bool FdnReverbAudioProcessor::acceptsMidi() const { return false; }
bool FdnReverbAudioProcessor::producesMidi() const { return false; }
bool FdnReverbAudioProcessor::isMidiEffect() const { return false; }
double FdnReverbAudioProcessor::getTailLengthSeconds() const { return 2.0; }
int FdnReverbAudioProcessor::getNumPrograms() { return (int)presets.size(); }
int FdnReverbAudioProcessor::getCurrentProgram() { return 0; }
void FdnReverbAudioProcessor::setCurrentProgram(int index) { loadPreset(index); }
const juce::String FdnReverbAudioProcessor::getProgramName(int index) {
    if (index >= 0 && index < (int)presets.size()) return presets[index].name;
    return {};
}
void FdnReverbAudioProcessor::changeProgramName(int index, const juce::String& newName) {}

bool FdnReverbAudioProcessor::hasEditor() const { return true; }
juce::AudioProcessorEditor* FdnReverbAudioProcessor::createEditor() { return new FdnReverbAudioProcessorEditor(*this); }

void FdnReverbAudioProcessor::getStateInformation(juce::MemoryBlock& destData) {
    auto state = parameters.copyState();
    state.setProperty("currentPresetIndex", currentPresetIndex, nullptr);
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void FdnReverbAudioProcessor::setStateInformation(const void* data, int sizeInBytes) {
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));
    if (xmlState.get() != nullptr)
        if (xmlState->hasTagName(parameters.state.getType())) {
            auto tree = juce::ValueTree::fromXml(*xmlState);
            parameters.replaceState(tree);
            if (tree.hasProperty("currentPresetIndex")) {
                currentPresetIndex = tree.getProperty("currentPresetIndex");
            }
        }
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool FdnReverbAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const {
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono() &&
        layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
    return true;
}
#endif

// ==============================================================================
// 4. REALTIME SAFE PROCESSING
// ==============================================================================
void FdnReverbAudioProcessor::releaseResources() {
    oversampling2x.reset();
    oversampling4x.reset();
}

void FdnReverbAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock) {
    storedSampleRate = sampleRate;
    storedBlockSize = samplesPerBlock;

    int factor = 0;
    int qualityIdx = (int)qualityParam->load();
    if (qualityIdx == 1) factor = 1;
    else if (qualityIdx == 2) factor = 2;

    oversampling2x = std::make_unique<juce::dsp::Oversampling<float>>(2, 1, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true);
    oversampling2x->initProcessing(samplesPerBlock);

    oversampling4x = std::make_unique<juce::dsp::Oversampling<float>>(2, 2, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true);
    oversampling4x->initProcessing(samplesPerBlock);

    if (factor == 1) currentOversampling = oversampling2x.get();
    else if (factor == 2) currentOversampling = oversampling4x.get();
    else currentOversampling = nullptr;

    currentOversamplingFactor = factor;

    float maxPossibleSampleRate = (float)sampleRate * 4.0f;
    fdnEngine.prepare(maxPossibleSampleRate);

    float dspSampleRate = (float)sampleRate * (float)(1 << factor);
    fdnEngine.prepare(dspSampleRate);
    storedDspSampleRate = dspSampleRate;
    lastQualityFactor = factor;

    if (currentOversampling) setLatencySamples(currentOversampling->getLatencyInSamples());
    else setLatencySamples(0);

    forceUpdate = true;
}

void FdnReverbAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages) {
    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();
    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear(i, 0, buffer.getNumSamples());

    if (panicTriggered.exchange(false)) {
        fdnEngine.reset();
        forceUpdate = true;
        return;
    }

    int qualityIdx = (int)qualityParam->load();
    int factor = 0;
    if (qualityIdx == 1) factor = 1;
    else if (qualityIdx == 2) factor = 2;

    if (currentOversamplingFactor != factor) {
        currentOversamplingFactor = factor;
        if (factor == 1)      currentOversampling = oversampling2x.get();
        else if (factor == 2) currentOversampling = oversampling4x.get();
        else                  currentOversampling = nullptr;

        if (currentOversampling) currentOversampling->reset();
        if (currentOversampling) setLatencySamples(currentOversampling->getLatencyInSamples());
        else setLatencySamples(0);

        forceUpdate = true;
    }

    float dspSampleRate = (float)getSampleRate() * (float)(1 << factor);
    if (std::abs(storedDspSampleRate - dspSampleRate) > 1.0f || lastQualityFactor != factor) {
        fdnEngine.prepare(dspSampleRate);
        storedDspSampleRate = dspSampleRate;
        lastQualityFactor = factor;
        forceUpdate = true;
    }

    juce::dsp::AudioBlock<float> block(buffer);
    juce::dsp::AudioBlock<float> processBlock = block;
    juce::dsp::AudioBlock<float> upsampledBlock;

    if (currentOversampling != nullptr) {
        upsampledBlock = currentOversampling->processSamplesUp(block);
        processBlock = upsampledBlock;
    }

    // Load Basic Params
    float w = widthParam->load();
    float d = depthParam->load();
    float h = heightParam->load();
    int mf = (int)matFloorParam->load();
    int mc = (int)matCeilParam->load();
    int mws = (int)matWallSideParam->load();
    int mwfb = (int)matWallFBParam->load();
    float absOv = absorbParam->load();
    float mRate = modRateParam->load();
    float mDepth = modDepthParam->load();
    float mix = mixParam->load();
    float pre = *parameters.getRawParameterValue("predelay");
    float temp = tempParam->load();
    float hum = humParam->load();

    float inLC = inLCParam->load();
    float inHC = inHCParam->load();
    float outLC = outLCParam->load();
    float outHC = outHCParam->load();

    float distRatio = distParam->load();
    float dist = distRatio * d;
    if (dist < 0.5f) dist = 0.5f;
    float pan = panParam->load();
    float srcHRatio = heightSourceParam->load();
    float srcH = srcHRatio * h;
    if (srcH < 0.1f) srcH = 0.1f;

    int shape = (int)shapeParam->load();
    float diff = diffParam->load();
    float stW = widthStereoParam->load();
    float outLvl = levelParam->load();

    float density = densityParam->load();
    float drive = driveParam->load();
    float decay = decayParam->load();
    
    // Phase 181: Load New Parameters
    float dynamics = dynamicsParam->load();
    float tilt = tiltParam->load();
    
    // Phase 182: Load Advanced Dynamics Params
    float dynThresh = dynThreshParam->load();
    float dynRatio  = dynRatioParam->load();
    float dynAtt    = dynAttackParam->load();
    float dynRel    = dynReleaseParam->load();

    PhysicsState currentState = {
        w, d, h, mf, mc, mws, mwfb, absOv, mRate, mDepth, pre, temp, hum, mix,
        inLC, inHC, outLC, outHC, dist, pan, srcH, shape, diff, stW, outLvl,
        density, drive, decay, 
        dynamics, tilt, dynThresh, dynRatio, dynAtt, dynRel,
        (int)processBlock.getNumSamples()
    };

    if (forceUpdate || currentState != lastPhysicsState) {
        // Correct 34 Arguments Call for Phase 182
        fdnEngine.updatePhysics(
            w, d, h, mf, mc, mws, mwfb, absOv, mRate, mDepth, pre, temp, hum, mix, 
            inLC, inHC, outLC, outHC, dist, pan, srcH, shape, diff, stW, outLvl, 
            density, drive, dynamics, tilt, 
            dynThresh, dynRatio, dynAtt, dynRel, // New args
            processBlock.getNumSamples(), decay
        );
            
        lastPhysicsState = currentState;
        forceUpdate = false;
    }

    float* outL = processBlock.getChannelPointer(0);
    float* outR = (processBlock.getNumChannels() > 1) ? processBlock.getChannelPointer(1) : nullptr;
    float* inputs[] = { outL, outR };
    float* outputs[] = { outL, outR };

    fdnEngine.process(inputs, outputs, (int)processBlock.getNumSamples(), processBlock.getNumChannels());

    if (currentOversampling != nullptr) {
        currentOversampling->processSamplesDown(block);
    }

    float maxAmp = 0.0f;
    const float* readL = buffer.getReadPointer(0);
    int numSamps = buffer.getNumSamples();
    for (int i = 0; i < numSamps; i += 8) {
        float abs = std::abs(readL[i]);
        if (abs > maxAmp) maxAmp = abs;
    }
    currentOutputLevel.store(maxAmp, std::memory_order_relaxed);
}

// Preset Management Helpers
void FdnReverbAudioProcessor::loadPreset(int index) {
    if (index < 0 || index >= (int)presets.size()) return;
    const auto& p = presets[index];

    auto setVal = [&](const juce::String& id, float val) {
        auto* param = parameters.getParameter(id);
        if (param) param->setValueNotifyingHost(param->convertTo0to1(val));
    };

    setVal("room_width", p.width);
    setVal("room_depth", p.depth);
    setVal("room_height", p.height);
    setVal("mat_floor", (float)p.matFloor);
    setVal("mat_ceil", (float)p.matCeil);
    setVal("mat_wall_s", (float)p.matWallSide);
    setVal("mat_wall_fb", (float)p.matWallFB);
    setVal("absorption", p.absorption);
    setVal("mod_rate", p.modRate);
    setVal("mod_depth", p.modDepth);
    setVal("dry_wet", p.mix);
    setVal("temp", p.temp);
    setVal("humidity", p.humidity);
    setVal("in_lc", p.inLC);
    setVal("in_hc", p.inHC);
    setVal("out_lc", p.outLC);
    setVal("out_hc", p.outHC);
    setVal("dist", p.dist);
    setVal("pan", p.pan);
    setVal("src_height", p.sourceHeight);
    setVal("shape", (float)p.roomShape);
    setVal("diffusion", p.diffusion);
    setVal("width_st", p.stereoWidth);
    setVal("level", p.outputLevel);
    setVal("drive", p.drive);
    setVal("density", p.density);
    setVal("decay", p.decay);
    setVal("predelay", p.predelay);
    
    // New Params
    setVal("dynamics", p.dynamics);
    setVal("tilt", p.tilt);
    
    // Phase 182
    setVal("dyn_thresh", p.dynThreshold);
    setVal("dyn_ratio", p.dynRatio);
    setVal("dyn_attack", p.dynAttack);
    setVal("dyn_release", p.dynRelease);

    currentPresetIndex = index;
    currentPresetName = p.name;
}

void FdnReverbAudioProcessor::saveUserPreset(const juce::String& name) {
    ReverbPreset p;
    p.name = name;
    p.category = "User";
    p.description = "User Preset";

    auto getVal = [&](const juce::String& id) {
        auto* param = parameters.getParameter(id);
        return param ? param->convertFrom0to1(param->getValue()) : 0.0f;
    };

    p.width = getVal("room_width");
    p.depth = getVal("room_depth");
    p.height = getVal("room_height");
    p.matFloor = (int)getVal("mat_floor");
    p.matCeil = (int)getVal("mat_ceil");
    p.matWallSide = (int)getVal("mat_wall_s");
    p.matWallFB = (int)getVal("mat_wall_fb");
    p.absorption = getVal("absorption");
    p.modRate = getVal("mod_rate");
    p.modDepth = getVal("mod_depth");
    p.mix = getVal("dry_wet");
    p.temp = getVal("temp");
    p.humidity = getVal("humidity");
    p.inLC = getVal("in_lc");
    p.inHC = getVal("in_hc");
    p.outLC = getVal("out_lc");
    p.outHC = getVal("out_hc");
    p.dist = getVal("dist");
    p.pan = getVal("pan");
    p.sourceHeight = getVal("src_height");
    p.roomShape = (int)getVal("shape");
    p.diffusion = getVal("diffusion");
    p.stereoWidth = getVal("width_st");
    p.outputLevel = getVal("level");
    p.drive = getVal("drive");
    p.density = getVal("density");
    p.decay = getVal("decay");
    p.predelay = getVal("predelay");
    
    // New Params
    p.dynamics = getVal("dynamics");
    p.tilt = getVal("tilt");
    p.dynThreshold = getVal("dyn_thresh");
    p.dynRatio = getVal("dyn_ratio");
    p.dynAttack = getVal("dyn_attack");
    p.dynRelease = getVal("dyn_release");

    juce::XmlElement xml("UserPreset");
    xml.setAttribute("name", p.name);
    xml.setAttribute("width", p.width);
    xml.setAttribute("depth", p.depth);
    xml.setAttribute("height", p.height);
    xml.setAttribute("matFloor", p.matFloor);
    xml.setAttribute("matCeil", p.matCeil);
    xml.setAttribute("matWallSide", p.matWallSide);
    xml.setAttribute("matWallFB", p.matWallFB);
    xml.setAttribute("absorption", p.absorption);
    xml.setAttribute("modRate", p.modRate);
    xml.setAttribute("modDepth", p.modDepth);
    xml.setAttribute("mix", p.mix);
    xml.setAttribute("temp", p.temp);
    xml.setAttribute("humidity", p.humidity);
    xml.setAttribute("inLC", p.inLC);
    xml.setAttribute("inHC", p.inHC);
    xml.setAttribute("outLC", p.outLC);
    xml.setAttribute("outHC", p.outHC);
    xml.setAttribute("dist", p.dist);
    xml.setAttribute("pan", p.pan);
    xml.setAttribute("srcHeight", p.sourceHeight);
    xml.setAttribute("shape", p.roomShape);
    xml.setAttribute("diffusion", p.diffusion);
    xml.setAttribute("stereoWidth", p.stereoWidth);
    xml.setAttribute("level", p.outputLevel);
    xml.setAttribute("drive", p.drive);
    xml.setAttribute("density", p.density);
    xml.setAttribute("decay", p.decay);
    xml.setAttribute("predelay", p.predelay);
    xml.setAttribute("dynamics", p.dynamics);
    xml.setAttribute("tilt", p.tilt);
    xml.setAttribute("dyn_thresh", p.dynThreshold);
    xml.setAttribute("dyn_ratio", p.dynRatio);
    xml.setAttribute("dyn_attack", p.dynAttack);
    xml.setAttribute("dyn_release", p.dynRelease);

    auto folder = getUserPresetFolder();
    if (!folder.exists()) folder.createDirectory();
    auto file = folder.getChildFile(name + ".xml");
    xml.writeTo(file);
}

void FdnReverbAudioProcessor::loadUserPreset(const juce::File& file) {
    auto xml = juce::parseXML(file);
    if (xml != nullptr && xml->hasTagName("UserPreset")) {
        ReverbPreset p;
        p.name = xml->getStringAttribute("name");
        p.category = "User";
        p.width = (float)xml->getDoubleAttribute("width", 10.0);
        p.depth = (float)xml->getDoubleAttribute("depth", 10.0);
        p.height = (float)xml->getDoubleAttribute("height", 5.0);
        p.matFloor = xml->getIntAttribute("matFloor", 0);
        p.matCeil = xml->getIntAttribute("matCeil", 0);
        p.matWallSide = xml->getIntAttribute("matWallSide", 0);
        p.matWallFB = xml->getIntAttribute("matWallFB", 0);
        p.absorption = (float)xml->getDoubleAttribute("absorption", 0.5);
        p.modRate = (float)xml->getDoubleAttribute("modRate", 0.5);
        p.modDepth = (float)xml->getDoubleAttribute("modDepth", 0.2);
        p.mix = (float)xml->getDoubleAttribute("mix", 0.3);
        p.temp = (float)xml->getDoubleAttribute("temp", 20.0);
        p.humidity = (float)xml->getDoubleAttribute("humidity", 50.0);
        p.inLC = (float)xml->getDoubleAttribute("inLC", 20.0);
        p.inHC = (float)xml->getDoubleAttribute("inHC", 20000.0);
        p.outLC = (float)xml->getDoubleAttribute("outLC", 20.0);
        p.outHC = (float)xml->getDoubleAttribute("outHC", 20000.0);
        p.dist = (float)xml->getDoubleAttribute("dist", 0.5);
        p.pan = (float)xml->getDoubleAttribute("pan", 0.0);
        p.sourceHeight = (float)xml->getDoubleAttribute("srcHeight", 0.5);
        p.roomShape = xml->getIntAttribute("shape", 0);
        p.diffusion = (float)xml->getDoubleAttribute("diffusion", 0.8);
        p.stereoWidth = (float)xml->getDoubleAttribute("stereoWidth", 1.0);
        p.outputLevel = (float)xml->getDoubleAttribute("level", 1.0);
        p.drive = (float)xml->getDoubleAttribute("drive", 0.0);
        p.density = (float)xml->getDoubleAttribute("density", 0.0);
        p.decay = (float)xml->getDoubleAttribute("decay", 1.0);
        p.predelay = (float)xml->getDoubleAttribute("predelay", 0.0);
        
        p.dynamics = (float)xml->getDoubleAttribute("dynamics", 0.0);
        p.tilt = (float)xml->getDoubleAttribute("tilt", 0.0);
        p.dynThreshold = (float)xml->getDoubleAttribute("dyn_thresh", -20.0);
        p.dynRatio = (float)xml->getDoubleAttribute("dyn_ratio", 2.0);
        p.dynAttack = (float)xml->getDoubleAttribute("dyn_attack", 10.0);
        p.dynRelease = (float)xml->getDoubleAttribute("dyn_release", 100.0);

        // Apply
        auto setVal = [&](const juce::String& id, float val) {
            auto* param = parameters.getParameter(id);
            if (param) param->setValueNotifyingHost(param->convertTo0to1(val));
        };
        setVal("room_width", p.width);
        setVal("room_depth", p.depth);
        setVal("room_height", p.height);
        setVal("mat_floor", (float)p.matFloor);
        setVal("mat_ceil", (float)p.matCeil);
        setVal("mat_wall_s", (float)p.matWallSide);
        setVal("mat_wall_fb", (float)p.matWallFB);
        setVal("absorption", p.absorption);
        setVal("mod_rate", p.modRate);
        setVal("mod_depth", p.modDepth);
        setVal("dry_wet", p.mix);
        setVal("temp", p.temp);
        setVal("humidity", p.humidity);
        setVal("in_lc", p.inLC);
        setVal("in_hc", p.inHC);
        setVal("out_lc", p.outLC);
        setVal("out_hc", p.outHC);
        setVal("dist", p.dist);
        setVal("pan", p.pan);
        setVal("src_height", p.sourceHeight);
        setVal("shape", (float)p.roomShape);
        setVal("diffusion", p.diffusion);
        setVal("width_st", p.stereoWidth);
        setVal("level", p.outputLevel);
        setVal("drive", p.drive);
        setVal("density", p.density);
        setVal("decay", p.decay);
        setVal("predelay", p.predelay);
        setVal("dynamics", p.dynamics);
        setVal("tilt", p.tilt);
        setVal("dyn_thresh", p.dynThreshold);
        setVal("dyn_ratio", p.dynRatio);
        setVal("dyn_attack", p.dynAttack);
        setVal("dyn_release", p.dynRelease);

        currentPresetName = p.name;
    }
}

juce::File FdnReverbAudioProcessor::getUserPresetFolder() const {
    auto docs = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory);
    return docs.getChildFile("FDN_Reverb_Presets");
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() {
    return new FdnReverbAudioProcessor();
}