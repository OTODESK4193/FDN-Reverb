/*
  ==============================================================================
    PluginEditor.cpp
    Phase 186: InfoBar Logic Restoration

    FIXES:
    - Restored mouseEnter logic to show descriptions for Shape and Material ComboBoxes.
  ==============================================================================
*/
#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cmath>

static juce::String utf8(const char* text) {
    if (text == nullptr) return {};
    return juce::String::fromUTF8(text);
}

static void makeImageTransparent(juce::Image& img) {
    if (!img.isValid()) return;
    img = img.createCopy();
    juce::Image::BitmapData bd(img, juce::Image::BitmapData::readWrite);

    for (int y = 0; y < img.getHeight(); ++y) {
        for (int x = 0; x < img.getWidth(); ++x) {
            auto c = bd.getPixelColour(x, y);
            if (c.getSaturation() < 0.1f && c.getBrightness() > 0.9f) {
                bd.setPixelColour(x, y, juce::Colours::transparentBlack);
            }
            else if (c.getSaturation() < 0.1f && c.getBrightness() > 0.8f && c.getBrightness() < 0.95f) {
                bd.setPixelColour(x, y, juce::Colours::transparentBlack);
            }
        }
    }
}

static juce::String getMaterialDescription(int index) {
    switch (index) {
    case 0: return utf8(u8"Concrete (Rough): 硬質で長い残響。");
    case 1: return utf8(u8"Concrete (Block): 低域を共鳴吸収。");
    case 2: return utf8(u8"Wood (Varnished): 明るく音楽的な響き。");
    case 3: return utf8(u8"Wood (Parquet): 低～中域の吸収。");
    case 4: return utf8(u8"Carpet (Heavy): 強力な高域吸収。");
    case 5: return utf8(u8"Curtain (Velvet): 中高域を強力吸収。");
    case 6: return utf8(u8"Acoustic Tile: 全帯域で高い吸収率。");
    case 7: return utf8(u8"Brick Wall: 密度感のある反射音。");
    case 8: return utf8(u8"Glass Window: 鋭い高域反射。");
    case 9: return utf8(u8"Metal (Sheet): 金属的なリンギング。");
    case 10: return utf8(u8"Water Surface: 平滑で重い反射。");
    case 11: return utf8(u8"Marble Floor: 豪華な全帯域反射。");
    case 12: return utf8(u8"Space (Void): 理論上の無限保持。");
    case 13: return utf8(u8"Vocal Tract: [SFX] 声道のような有機的な共鳴。");
    case 14: return utf8(u8"Tatami: 高域を吸う静かな響き。");
    case 15: return utf8(u8"Acrylic: 硬質でクリアな反射。");
    case 16: return utf8(u8"Carbon Fiber: ドライでタイトな響き。");
    case 17: return utf8(u8"Fresh Snow: 高域を完全に吸収する静寂。");
    case 18: return utf8(u8"Forest Floor: 複雑な散乱と吸収。");
    case 19: return utf8(u8"Cave (Limestone): 湿った重厚な響き。");
    case 20: return utf8(u8"Muscle Tissue: [SFX] 衝撃を吸収するデッドな質感。");
    case 21: return utf8(u8"Blubber: 高粘度の液体的な減衰。");
    case 22: return utf8(u8"Shoji (Rice Paper): 柔らかく温かい響き。");
    case 23: return utf8(u8"Double Glazing: 特定低域で共鳴する。");
    case 24: return utf8(u8"Bookshelf: ランダムな拡散と吸音。");
    case 25: return utf8(u8"Heavy Curtain: 強い高域減衰と閉塞感。");
    case 26: return utf8(u8"Ice Sheet: 極めて鋭い高域反射。");
    case 27: return utf8(u8"Magma: 低域が重く粘る。");
    case 28: return utf8(u8"Sand Dune: 粒子状の拡散と吸収。");
    case 29: return utf8(u8"Swamp: [SFX] 不安定に揺らぐ液状空間。");
    case 30: return utf8(u8"Aerogel: 音速が変化する不思議な空間。");
    case 31: return utf8(u8"Plasma Field: [SFX] 激しく歪む電気的空間。");
    case 32: return utf8(u8"Neutron Star: 超高密度の硬質な反射。");
    case 33: return utf8(u8"Force Field: [SFX] 特定周波数を弾くバリア。");
    default: return utf8(u8"材質を選択してください。");
    }
}

static juce::String getShapeDescription(int index) {
    switch (index) {
    case 0: return utf8(u8"Shoe-box: 標準的な長方形。自然な響き。");
    case 1: return utf8(u8"Dome: ドーム状。焦点のある響き。");
    case 2: return utf8(u8"Fan: 扇形。後方へ拡散する。");
    case 3: return utf8(u8"Cylinder: 円筒形。金属的な共鳴。");
    case 4: return utf8(u8"Pyramid: 鋭角な天井。不均一な響き。");
    case 5: return utf8(u8"Tesseract: 4次元。超高密度拡散。");
    case 6: return utf8(u8"Chaos: ランダム構造。有機的なカオス。");
    default: return "";
    }
}

// ==============================================================================
// Absorption Graph Implementation
// ==============================================================================
void AbsorptionGraph::timerCallback() {
    repaint();
}

void AbsorptionGraph::paint(juce::Graphics& g) {
    g.fillAll(juce::Colour(0xff252525));
    auto bounds = getLocalBounds().toFloat();
    float w = bounds.getWidth();
    float h = bounds.getHeight();
    float padding = 20.0f;
    float chartW = w - 2 * padding;
    float stepX = chartW / 5.0f;
    RT60Data rt60 = processor.getRT60();

    // Check if RT60 is valid (non-zero)
    bool hasSignal = false;
    for (float v : rt60.decay) { if (v > 0.01f) hasSignal = true; }

    auto norm = [h](float val) {
        float clamped = std::clamp(val, 0.0f, 10.0f);
        return h - 40.0f - (clamped / 10.0f) * (h - 60.0f);
        };
    g.setColour(juce::Colour(0xff555555));
    for (int i = 1; i <= 5; ++i) {
        float gy = h - 40.0f - (i * 2.0f / 10.0f) * (h - 60.0f);
        g.drawLine(padding, gy, w - padding, gy, 0.5f);
    }

    if (hasSignal) {
        juce::Path strokePath;
        juce::Path fillPath;
        float startX = padding;
        float startY = norm(rt60.decay[0]);
        strokePath.startNewSubPath(startX, startY);
        fillPath.startNewSubPath(startX, h);
        fillPath.lineTo(startX, startY);
        for (int i = 1; i < 6; ++i) {
            float x = padding + i * stepX;
            float y = norm(rt60.decay[i]);
            strokePath.lineTo(x, y);
            fillPath.lineTo(x, y);
        }
        fillPath.lineTo(padding + 5 * stepX, h);
        fillPath.closeSubPath();
        float level = processor.currentOutputLevel.load();
        float glowAlpha = std::clamp(level * 2.5f, 0.0f, 1.0f) * 0.5f + 0.2f;
        float lineThick = 3.0f + level * 3.0f;
        juce::Colour cLow = juce::Colours::darkorange;
        juce::Colour cMid = juce::Colours::orange;
        juce::Colour cHigh = juce::Colours::yellow;
        juce::ColourGradient fillGrad(cLow.withAlpha(glowAlpha), padding, 0, cHigh.withAlpha(glowAlpha), w - padding, 0, false);
        fillGrad.addColour(0.5, cMid.withAlpha(glowAlpha));
        g.setGradientFill(fillGrad);
        g.fillPath(fillPath);
        juce::ColourGradient lineGrad(cLow, padding, 0, cHigh, w - padding, 0, false);
        lineGrad.addColour(0.5, cMid);
        g.setGradientFill(lineGrad);
        g.strokePath(strokePath, juce::PathStrokeType(lineThick, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        float dotSize = 8.0f + level * 4.0f;
        for (int i = 0; i < 6; ++i) {
            float x = padding + i * stepX;
            float y = norm(rt60.decay[i]);
            juce::Colour c = (i < 2) ? cLow : ((i < 4) ? cMid : cHigh);
            g.setColour(c);
            g.fillEllipse(x - dotSize / 2, y - dotSize / 2, dotSize, dotSize);
        }
    }
    else {
        // SFX Mode Indicator
        g.setColour(juce::Colours::orange.withAlpha(0.5f));
        g.setFont(20.0f);
        g.drawText("SFX RESONATOR MODE", getLocalBounds(), juce::Justification::centred);
    }

    g.setColour(juce::Colours::white);
    g.setFont(14.0f);
    g.drawText("Estimated RT60 (6-Band)", 5, 5, 200, 20, juce::Justification::topLeft);
    g.setFont(12.0f);
    const char* labels[] = { "125", "250", "500", "1k", "2k", "4k" };
    for (int i = 0; i < 6; ++i) {
        float x = padding + i * stepX;
        g.drawText(labels[i], (int)x - 20, (int)h - 20, 40, 20, juce::Justification::centred);
    }
}

// ==============================================================================
// DynamicsPanel Implementation
// ==============================================================================
DynamicsPanel::DynamicsPanel(FdnReverbAudioProcessor& p) : processor(p) {
    setupSlider(threshSlider, "dyn_thresh", "Threshold", " dB");
    setupSlider(ratioSlider, "dyn_ratio", "Ratio", ":1");
    setupSlider(attackSlider, "dyn_attack", "Attack", " ms");
    setupSlider(releaseSlider, "dyn_release", "Release", " ms");

    auto* l = new juce::Label();
    l->setText("Advanced Dynamics", juce::dontSendNotification);
    l->setFont(juce::Font("Arial", 16.0f, juce::Font::bold));
    l->setJustificationType(juce::Justification::centred);
    l->setColour(juce::Label::textColourId, juce::Colours::orange);
    addAndMakeVisible(l);
    labels.emplace_back(l);
}

DynamicsPanel::~DynamicsPanel() {
    attachments.clear();
}

void DynamicsPanel::setupSlider(juce::Slider& s, const juce::String& paramID, const juce::String& name, const juce::String& suffix) {
    addAndMakeVisible(s);
    s.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    s.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 20);
    s.setColour(juce::Slider::rotarySliderFillColourId, juce::Colour(0xffffa500));
    s.setColour(juce::Slider::thumbColourId, juce::Colours::white);
    s.setColour(juce::Slider::textBoxTextColourId, juce::Colour(0xffffa500));
    s.setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);

    auto* l = new juce::Label();
    l->setText(name, juce::dontSendNotification);
    l->setFont(13.0f);
    l->setJustificationType(juce::Justification::centred);
    l->setColour(juce::Label::textColourId, juce::Colours::lightgrey);
    addAndMakeVisible(l);
    labels.emplace_back(l);

    attachments.push_back(std::make_unique<Attachment>(processor.parameters, paramID, s));

    if (suffix.isNotEmpty()) {
        s.setTextValueSuffix(suffix);
    }
}

void DynamicsPanel::paint(juce::Graphics& g) {
    g.fillAll(juce::Colour(0xff252525));
    g.setColour(juce::Colour(0xff505050));
    g.drawRect(getLocalBounds(), 2);
}

void DynamicsPanel::resized() {
    auto area = getLocalBounds().reduced(10);
    if (!labels.empty()) {
        labels.back()->setBounds(area.removeFromTop(25));
    }
    int w = area.getWidth() / 2;
    int h = area.getHeight() / 2;
    int sliderSize = std::min(w, h) - 20;
    auto place = [&](juce::Slider& s, int idx) {
        int row = idx / 2;
        int col = idx % 2;
        int x = area.getX() + col * w + (w - sliderSize) / 2;
        int y = area.getY() + row * h + 15;
        if (idx < (int)labels.size()) {
            labels[idx]->setBounds(area.getX() + col * w, y - 18, w, 18);
        }
        s.setBounds(x, y, sliderSize, sliderSize);
        };
    place(threshSlider, 0);
    place(ratioSlider, 1);
    place(attackSlider, 2);
    place(releaseSlider, 3);
}

// ==============================================================================
// RoomVisualizer
// ==============================================================================
void RoomVisualizer::timerCallback() {
    repaint();
}

void RoomVisualizer::paint(juce::Graphics& g) {
    g.fillAll(juce::Colour(0xff252525));

    int shape = (int)*processor.parameters.getRawParameterValue("shape");
    float w = *processor.parameters.getRawParameterValue("room_width");
    float d = *processor.parameters.getRawParameterValue("room_depth");
    float h = *processor.parameters.getRawParameterValue("room_height");

    float maxDim = std::max({ w, d, h });
    float normW = (w / maxDim);
    float normD = (d / maxDim);
    float normH = (h / maxDim);

    float distRatio = *processor.parameters.getRawParameterValue("dist");
    float panVal = *processor.parameters.getRawParameterValue("pan");
    float srcHRatio = *processor.parameters.getRawParameterValue("src_height");

    float srcY = (srcHRatio - 0.5f) * normH;
    float maxR = std::min(normW, normD) * 0.5f;
    float safeDist = distRatio * maxR * 0.95f;

    float panAngle = panVal * 0.7f;
    float srcX = safeDist * std::sin(panAngle);
    float srcZ = safeDist * std::cos(panAngle);

    float audioLevel = processor.currentOutputLevel.load();
    float pulse = 1.0f + audioLevel * 0.15f;

    std::vector<Point3D> vertices;
    std::vector<std::pair<int, int>> edges;
    auto addLine = [&](int a, int b) { edges.push_back({ a, b }); };

    float hw = normW * 0.5f;
    float hh = normH * 0.5f;
    float hd = normD * 0.5f;

    switch (shape) {
    case 0: // Shoe-box
    default:
        vertices.push_back({ -hw, -hh, -hd }); vertices.push_back({ hw, -hh, -hd });
        vertices.push_back({ hw,  hh, -hd }); vertices.push_back({ -hw,  hh, -hd });
        vertices.push_back({ -hw, -hh,  hd }); vertices.push_back({ hw, -hh,  hd });
        vertices.push_back({ hw,  hh,  hd }); vertices.push_back({ -hw,  hh,  hd });
        addLine(0, 1); addLine(1, 2); addLine(2, 3); addLine(3, 0);
        addLine(4, 5); addLine(5, 6); addLine(6, 7); addLine(7, 4);
        addLine(0, 4); addLine(1, 5); addLine(2, 6); addLine(3, 7);
        break;

    case 1: // Dome
    case 3: // Cylinder
    {
        int segs = 8;
        for (int i = 0; i < segs; ++i) {
            float angle = (float)i / segs * 6.28318f;
            float x = std::cos(angle) * hw;
            float z = std::sin(angle) * hd;
            vertices.push_back({ x, -hh, z });

            if (shape == 1)
                vertices.push_back({ x * 0.6f, hh, z * 0.6f });
            else
                vertices.push_back({ x, hh, z });
        }
        if (shape == 1) vertices.push_back({ 0, hh * 1.2f, 0 });

        for (int i = 0; i < segs; ++i) {
            int next = (i + 1) % segs;
            int b1 = i * 2; int t1 = i * 2 + 1;
            int b2 = next * 2; int t2 = next * 2 + 1;
            addLine(b1, b2);
            addLine(t1, t2);
            addLine(b1, t1);
            if (shape == 1) addLine(t1, (int)vertices.size() - 1);
        }
    }
    break;

    case 2: // Fan
        vertices.push_back({ -hw * 0.4f, -hh, -hd }); vertices.push_back({ hw * 0.4f, -hh, -hd });
        vertices.push_back({ hw * 0.4f,  hh, -hd }); vertices.push_back({ -hw * 0.4f,  hh, -hd });
        vertices.push_back({ -hw, -hh,  hd }); vertices.push_back({ hw, -hh,  hd });
        vertices.push_back({ hw,  hh,  hd }); vertices.push_back({ -hw,  hh,  hd });
        addLine(0, 1); addLine(1, 2); addLine(2, 3); addLine(3, 0);
        addLine(4, 5); addLine(5, 6); addLine(6, 7); addLine(7, 4);
        addLine(0, 4); addLine(1, 5); addLine(2, 6); addLine(3, 7);
        break;

    case 4: // Pyramid
        vertices.push_back({ -hw, -hh, -hd }); vertices.push_back({ hw, -hh, -hd });
        vertices.push_back({ hw, -hh,  hd }); vertices.push_back({ -hw, -hh,  hd });
        vertices.push_back({ 0,  hh,  0 });
        addLine(0, 1); addLine(1, 2); addLine(2, 3); addLine(3, 0);
        addLine(0, 4); addLine(1, 4); addLine(2, 4); addLine(3, 4);
        break;

    case 5: // Tesseract
    {
        vertices.push_back({ -hw, -hh, -hd }); vertices.push_back({ hw, -hh, -hd });
        vertices.push_back({ hw,  hh, -hd }); vertices.push_back({ -hw,  hh, -hd });
        vertices.push_back({ -hw, -hh,  hd }); vertices.push_back({ hw, -hh,  hd });
        vertices.push_back({ hw,  hh,  hd }); vertices.push_back({ -hw,  hh,  hd });
        float s = 0.5f;
        vertices.push_back({ -hw * s, -hh * s, -hd * s }); vertices.push_back({ hw * s, -hh * s, -hd * s });
        vertices.push_back({ hw * s,  hh * s, -hd * s }); vertices.push_back({ -hw * s,  hh * s, -hd * s });
        vertices.push_back({ -hw * s, -hh * s,  hd * s }); vertices.push_back({ hw * s, -hh * s,  hd * s });
        vertices.push_back({ hw * s,  hh * s,  hd * s }); vertices.push_back({ -hw * s,  hh * s,  hd * s });

        auto addCube = [&](int offset) {
            addLine(0 + offset, 1 + offset); addLine(1 + offset, 2 + offset); addLine(2 + offset, 3 + offset); addLine(3 + offset, 0 + offset);
            addLine(4 + offset, 5 + offset); addLine(5 + offset, 6 + offset); addLine(6 + offset, 7 + offset); addLine(7 + offset, 4 + offset);
            addLine(0 + offset, 4 + offset); addLine(1 + offset, 5 + offset); addLine(2 + offset, 6 + offset); addLine(3 + offset, 7 + offset);
            };
        addCube(0);
        addCube(8);
        for (int i = 0; i < 8; ++i) addLine(i, i + 8);
    }
    break;

    case 6: // Chaos
        for (int i = 0; i < 8; ++i) {
            float ox = std::sin((float)i * 1.5f) * hw * 0.3f;
            float oy = std::cos((float)i * 2.3f) * hh * 0.3f;
            float oz = std::sin((float)i * 3.7f) * hd * 0.3f;
            float bx = (i & 1) ? hw : -hw;
            float by = (i & 2) ? hh : -hh;
            float bz = (i & 4) ? hd : -hd;
            vertices.push_back({ bx + ox, by + oy, bz + oz });
        }
        addLine(0, 1); addLine(1, 2); addLine(2, 3); addLine(3, 0);
        addLine(4, 5); addLine(5, 6); addLine(6, 7); addLine(7, 4);
        addLine(0, 4); addLine(1, 5); addLine(2, 6); addLine(3, 7);
        break;
    }

    auto bounds = getLocalBounds().toFloat();
    float cx = bounds.getCentreX();
    float cy = bounds.getCentreY();
    float scale = std::min(bounds.getWidth(), bounds.getHeight()) * 1.4f * pulse;
    float fixedRotation = 0.4f;
    float camDist = 2.5f;
    float cosA = std::cos(fixedRotation);
    float sinA = std::sin(fixedRotation);

    auto project = [&](Point3D p) -> juce::Point<float> {
        float rx = p.x * cosA - p.z * sinA;
        float rz = p.x * sinA + p.z * cosA;
        float ry = p.y;
        float z = rz + camDist;
        if (z < 0.1f) z = 0.1f;
        float screenX = cx + (rx / z) * scale;
        float screenY = cy - (ry / z) * scale;
        return { screenX, screenY };
        };

    g.setColour(juce::Colour(0xff00b5ff).withAlpha(0.6f + audioLevel * 0.4f));
    for (auto& edge : edges) {
        auto p1 = project(vertices[edge.first]);
        auto p2 = project(vertices[edge.second]);
        g.drawLine(p1.x, p1.y, p2.x, p2.y, 2.0f);
    }

    Point3D srcP = { srcX, srcY, srcZ };
    auto projSrc = project(srcP);
    float dotSize = 10.0f + audioLevel * 8.0f;
    g.setColour(juce::Colours::orange.withAlpha(0.9f));
    g.fillEllipse(projSrc.x - dotSize / 2, projSrc.y - dotSize / 2, dotSize, dotSize);
    g.setColour(juce::Colours::white);
    g.setFont(12.0f);
    g.drawText("Src", (int)projSrc.x + 8, (int)projSrc.y - 12, 30, 15, juce::Justification::left);

    juce::String shapeName = "";
    switch (shape) {
    case 0: shapeName = "Shoe-box"; break;
    case 1: shapeName = "Dome"; break;
    case 2: shapeName = "Fan"; break;
    case 3: shapeName = "Cylinder"; break;
    case 4: shapeName = "Pyramid"; break;
    case 5: shapeName = "Tesseract"; break;
    case 6: shapeName = "Chaos"; break;
    default: shapeName = "Room"; break;
    }
    g.setColour(juce::Colours::white.withAlpha(0.7f));
    g.setFont(14.0f);
    g.drawText("3D: " + shapeName, 5, 5, 200, 20, juce::Justification::topLeft);
}

// ==============================================================================
// EDITOR IMPLEMENTATION
// ==============================================================================

FdnReverbAudioProcessorEditor::FdnReverbAudioProcessorEditor(FdnReverbAudioProcessor& p)
    : AudioProcessorEditor(&p), audioProcessor(p), roomVis(p), absorptionGraph(p), dynamicsPanel(p)
{
    juce::LookAndFeel::setDefaultLookAndFeel(&lightLookAndFeel);
    startTimer(100);

    logoImage = juce::ImageCache::getFromMemory(BinaryData::logo_png, BinaryData::logo_pngSize);
    makeImageTransparent(logoImage);

    addAndMakeVisible(infoBar);

    addAndMakeVisible(presetButton);
    presetButton.setButtonText(audioProcessor.getCurrentPresetName());
    presetButton.onClick = [this] { showPresetMenu(); };
    presetButton.addMouseListener(this, false);

    addAndMakeVisible(saveButton);
    saveButton.setButtonText("Save");
    saveButton.onClick = [this] {
        auto* w = new juce::AlertWindow("Save Preset", "Enter preset name:", juce::AlertWindow::NoIcon);
        w->addTextEditor("name", "My Preset", "Preset Name:");
        w->addButton("Save", 1);
        w->addButton("Cancel", 0);
        auto safeThis = juce::Component::SafePointer<FdnReverbAudioProcessorEditor>(this);
        w->enterModalState(true, juce::ModalCallbackFunction::create([safeThis, w](int result) {
            if (safeThis != nullptr && result == 1) {
                auto name = w->getTextEditorContents("name");
                if (name.isNotEmpty()) {
                    safeThis->audioProcessor.saveUserPreset(name);
                    safeThis->presetButton.setButtonText(name);
                    safeThis->infoBar.showInfo("User Preset Saved.");
                    safeThis->buildUserPresetMenu();
                }
            }
            delete w;
            }));
        };
    helpTexts[&saveButton] = utf8(u8"現在の設定をユーザープリセットとして保存します。");

    addAndMakeVisible(panicButton);
    panicButton.setButtonText("PANIC");
    panicButton.onClick = [this] { audioProcessor.triggerPanic(); };
    panicButton.addMouseListener(this, false);
    helpTexts[&panicButton] = utf8(u8"緊急リセット: 全てのオーディオ処理をリセットし、発振やノイズを止めます。");

    // Advanced Button
    addAndMakeVisible(advancedButton);
    advancedButton.setButtonText("Adv.");
    advancedButton.setTooltip("Advanced Dynamics Settings");
    advancedButton.onClick = [this] {
        dynamicsPanel.setVisible(!dynamicsPanel.isVisible());
        resized();
        };
    helpTexts[&advancedButton] = utf8(u8"ダイナミクス詳細設定パネルの表示/非表示を切り替えます。");

    addChildComponent(dynamicsPanel);
    dynamicsPanel.setVisible(false);

    addAndMakeVisible(rt60Label);
    rt60Label.setColour(juce::Label::textColourId, juce::Colours::black);
    rt60Label.setJustificationType(juce::Justification::centredRight);
    rt60Label.setFont(12.0f);

    addAndMakeVisible(roomVis);
    addAndMakeVisible(absorptionGraph);

    buildFactoryPresetMenu();
    buildUserPresetMenu();

    setupCombo(shapeBox, "shape", utf8(u8"Room Shape: 空間の形状と反射アルゴリズム。"));
    setupCombo(materialBox, "mat_floor", utf8(u8"Floor: 床の材質。"));
    setupCombo(ceilBox, "mat_ceil", utf8(u8"Ceiling: 天井の材質。"));
    setupCombo(wallSBox, "mat_wall_s", utf8(u8"Side Wall: 横壁の材質。"));
    setupCombo(wallFBBox, "mat_wall_fb", utf8(u8"F/B Wall: 前後の壁の材質。"));
    setupCombo(qualityBox, "quality", utf8(u8"Quality: オーバーサンプリング設定。"));

    setupSlider(widthSlider, "room_width", utf8(u8"Width: 部屋の幅。"));
    setupSlider(depthSlider, "room_depth", utf8(u8"Depth: 部屋の奥行。"));
    setupSlider(heightSlider, "room_height", utf8(u8"Height: 部屋の高さ。"));
    setupSlider(predelaySlider, "predelay", utf8(u8"Pre-Delay: 初期遅延。"));
    setupSlider(tempSlider, "temp", utf8(u8"Temp: 気温(空気吸収)。"));
    setupSlider(humSlider, "humidity", utf8(u8"Humidity: 湿度(空気吸収)。"));
    setupSlider(decaySlider, "decay", utf8(u8"Decay: 残響時間のスケーリング。"));

    setupSlider(distSlider, "dist", utf8(u8"Distance: 音源距離。"));
    setupSlider(panSlider, "pan", utf8(u8"Pan: 音源定位。"));
    setupSlider(srcHeightSlider, "src_height", utf8(u8"Src Height: 音源の高さ。"));
    setupSlider(modRateSlider, "mod_rate", utf8(u8"Mod Rate: 揺らぎ速度。"));
    setupSlider(modDepthSlider, "mod_depth", utf8(u8"Mod Depth: 揺らぎ深さ。"));
    setupSlider(diffSlider, "diffusion", utf8(u8"Diffusion: 拡散密度。"));
    setupSlider(densitySlider, "density", utf8(u8"Density: 粒子密度。"));

    setupSlider(absorbSlider, "absorption", utf8(u8"Absorption: 吸音率調整。"));
    setupSlider(driveSlider, "drive", utf8(u8"Drive: サチュレーション。"));
    setupSlider(widthStSlider, "width_st", utf8(u8"Stereo Width: ステレオ幅。"));
    setupSlider(levelSlider, "level", utf8(u8"Level: 出力レベル。"));
    setupSlider(mixSlider, "dry_wet", utf8(u8"Mix: ドライ/ウェット比。"));

    setupSlider(dynamicsSlider, "dynamics", utf8(u8"Dynamics Amount: ダッキング/ブルームの量。"));
    setupSlider(tiltSlider, "tilt", utf8(u8"Tilt EQ: 音色の明るさ調整。"));

    setupSlider(inLCSlider, "in_lc", utf8(u8"Input LowCut"));
    setupSlider(inHCSlider, "in_hc", utf8(u8"Input HighCut"));
    setupSlider(outLCSlider, "out_lc", utf8(u8"Output LowCut"));
    setupSlider(outHCSlider, "out_hc", utf8(u8"Output HighCut"));

    setSize(900, 700);
}

FdnReverbAudioProcessorEditor::~FdnReverbAudioProcessorEditor() {
    stopTimer();
    removeAllChildren();
    juce::LookAndFeel::setDefaultLookAndFeel(nullptr);
}

void FdnReverbAudioProcessorEditor::buildFactoryPresetMenu() {
    factoryPresetMenu.clear();
    auto& presets = audioProcessor.getPresets();
    if (presets.size() > 0) {
        factoryPresetMenu.addItem(1, presets[0].name);
        factoryPresetMenu.addSeparator();
    }
    std::map<juce::String, juce::PopupMenu> categories;
    for (size_t i = 1; i < presets.size(); ++i) {
        categories[presets[i].category].addItem((int)i + 1, presets[i].name);
    }
    for (auto& cat : categories) {
        factoryPresetMenu.addSubMenu(cat.first, cat.second);
    }
}

void FdnReverbAudioProcessorEditor::buildUserPresetMenu() {
    userPresetMenu.clear();
    auto userDir = audioProcessor.getUserPresetFolder();
    if (userDir.exists()) {
        auto files = userDir.findChildFiles(juce::File::findFiles, false, "*.xml");
        for (int i = 0; i < files.size(); ++i) {
            userPresetMenu.addItem(1000 + i, files[i].getFileNameWithoutExtension());
        }
    }
}

void FdnReverbAudioProcessorEditor::showPresetMenu() {
    juce::PopupMenu menu;
    menu = factoryPresetMenu;
    menu.addSeparator();
    menu.addSubMenu("User Presets", userPresetMenu);

    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(presetButton),
        [this](int result) {
            if (result == 0) return;
            if (result < 1000) {
                int idx = result - 1;
                audioProcessor.loadPreset(idx);
                presetButton.setButtonText(audioProcessor.getPresets()[idx].name);
                infoBar.showInfo(audioProcessor.getPresets()[idx].name + ": " + audioProcessor.getPresets()[idx].description);
            }
            else {
                auto userDir = audioProcessor.getUserPresetFolder();
                auto files = userDir.findChildFiles(juce::File::findFiles, false, "*.xml");
                int idx = result - 1000;
                if (idx < files.size()) {
                    audioProcessor.loadUserPreset(files[idx]);
                    presetButton.setButtonText(files[idx].getFileNameWithoutExtension());
                    infoBar.showInfo("User Preset Loaded.");
                }
            }
        });
}

void FdnReverbAudioProcessorEditor::setupSlider(juce::Slider& s, const juce::String& paramID, const juce::String& desc) {
    addAndMakeVisible(s);
    s.setSliderStyle(juce::Slider::LinearHorizontal);
    s.setTextBoxStyle(juce::Slider::TextBoxLeft, false, 60, 20);
    s.setColour(juce::Slider::textBoxTextColourId, juce::Colour(0xffffa500)); // FIX: Orange text
    s.setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);

    auto* l = new juce::Label();
    auto* param = audioProcessor.parameters.getParameter(paramID);
    if (param) {
        l->setText(param->getName(100), juce::dontSendNotification);
        sliderAtts.push_back(std::make_unique<Attachment>(audioProcessor.parameters, paramID, s));

        if (paramID.contains("hc") || paramID.contains("lc")) {
            s.textFromValueFunction = [](double value) {
                if (value >= 1000.0) return juce::String(value / 1000.0, 1) + " k";
                return juce::String(value, 0) + " Hz";
                };
            s.updateText();
        }
        if (paramID == "tilt") {
            s.textFromValueFunction = [](double value) {
                return (value > 0 ? "+" : "") + juce::String(value, 1) + " dB";
                };
        }
    }
    else {
        l->setText("Error: " + paramID, juce::dontSendNotification);
    }

    l->setFont(15.0f);
    l->setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(l);
    labels.emplace_back(l);

    s.addMouseListener(this, false);
    helpTexts[&s] = desc;
}

void FdnReverbAudioProcessorEditor::setupCombo(juce::ComboBox& box, const juce::String& paramID, const juce::String& desc, const std::vector<int>& indices) {
    addAndMakeVisible(box);
    auto* param = dynamic_cast<juce::AudioParameterChoice*>(audioProcessor.parameters.getParameter(paramID));
    if (param) {
        box.addItemList(param->choices, 1);
        comboAtts.push_back(std::make_unique<ComboAttachment>(audioProcessor.parameters, paramID, box));
    }

    auto* l = new juce::Label();
    if (param) l->setText(param->getName(100), juce::dontSendNotification);
    else l->setText("Error: " + paramID, juce::dontSendNotification);

    l->setFont(15.0f);
    l->setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(l);
    labels.emplace_back(l);

    box.addMouseListener(this, false);
    helpTexts[&box] = desc;
}

void FdnReverbAudioProcessorEditor::mouseEnter(const juce::MouseEvent& event) {
    auto* comp = event.eventComponent;

    // 1. Preset Button
    if (comp == &presetButton) {
        int idx = audioProcessor.getCurrentPresetIndex();
        if (idx >= 0 && idx < (int)audioProcessor.getPresets().size()) {
            infoBar.showInfo(audioProcessor.getPresets()[idx].description);
        }
        return;
    }
    // 2. Advanced Button
    if (comp == &advancedButton) {
        infoBar.showInfo(helpTexts[comp]);
        return;
    }

    // 3. FIX: Show Description for ComboBoxes (Shape / Material)
    if (comp == &shapeBox) {
        int idx = shapeBox.getSelectedId() - 1;
        infoBar.showInfo(getShapeDescription(idx));
        return;
    }
    if (comp == &materialBox || comp == &ceilBox || comp == &wallSBox || comp == &wallFBBox) {
        auto* box = dynamic_cast<juce::ComboBox*>(comp);
        if (box) {
            int idx = box->getSelectedId() - 1;
            infoBar.showInfo(getMaterialDescription(idx));
            return;
        }
    }

    // 4. Sliders and other components
    if (helpTexts.find(comp) != helpTexts.end()) {
        infoBar.showInfo(helpTexts[comp]);
    }
}

void FdnReverbAudioProcessorEditor::mouseExit(const juce::MouseEvent& event) {
    infoBar.showInfo("Ready.");
}

void FdnReverbAudioProcessorEditor::timerCallback() {
    RT60Data rt60 = audioProcessor.getRT60();

    juce::String text;
    text << "RT60(s): ";
    text << "125Hz:" << juce::String(rt60.decay[0], 1) << " ";
    text << "250:" << juce::String(rt60.decay[1], 1) << " ";
    text << "500:" << juce::String(rt60.decay[2], 1) << " ";
    text << "1k:" << juce::String(rt60.decay[3], 1) << " ";
    text << "2k:" << juce::String(rt60.decay[4], 1) << " ";
    text << "4k:" << juce::String(rt60.decay[5], 1);

    rt60Label.setText(text, juce::dontSendNotification);

    if (audioProcessor.getCurrentPresetName() != presetButton.getButtonText()) {
        presetButton.setButtonText(audioProcessor.getCurrentPresetName());
    }
}

void FdnReverbAudioProcessorEditor::paint(juce::Graphics& g) {
    g.fillAll(findColour(juce::ResizableWindow::backgroundColourId));

    g.setColour(juce::Colour(0xffe8e8e8));
    g.fillRect(0, 0, getWidth(), 40);

    if (logoImage.isValid()) {
        g.drawImageWithin(logoImage, 20, 0, 250, 40, juce::RectanglePlacement::xLeft | juce::RectanglePlacement::yMid, false);
    }
    else {
        g.setColour(juce::Colour(0xff202020));
        g.setFont(juce::Font("Arial", 22.0f, juce::Font::bold));
        g.drawText("FDN REVERB", 20, 0, 250, 40, juce::Justification::centredLeft);
    }

    g.setColour(juce::Colour(0xffffa500));
    g.fillRect(0, 38, getWidth(), 4);

    // Dim the background if overlay is visible
    if (dynamicsPanel.isVisible()) {
        g.setColour(juce::Colours::black.withAlpha(0.5f));
        g.fillRect(getLocalBounds().removeFromBottom(getHeight() - 42)); // Don't cover header
    }
}

void FdnReverbAudioProcessorEditor::resized() {
    auto area = getLocalBounds();

    infoBar.setBounds(area.removeFromBottom(50));

    auto header = area.removeFromTop(42);
    panicButton.setBounds(header.removeFromRight(80).reduced(5));
    saveButton.setBounds(header.removeFromRight(60).reduced(5));
    presetButton.setBounds(header.removeFromRight(200).reduced(5));
    rt60Label.setBounds(header.removeFromRight(400).reduced(5));

    auto visArea = area.removeFromTop(220);
    int visWidth = visArea.getWidth() / 2;
    roomVis.setBounds(visArea.removeFromLeft(visWidth).reduced(5));
    absorptionGraph.setBounds(visArea.reduced(5));

    auto filterArea = area.removeFromBottom(60);
    int filterW = filterArea.getWidth() / 4;

    auto leftSidebar = area.removeFromLeft(220);
    int comboH = 45;
    size_t labelIdx = 0;

    auto placeCombo = [&](juce::Component& comp) {
        auto slot = leftSidebar.removeFromTop(comboH);
        if (labelIdx < labels.size()) {
            labels[labelIdx]->setBounds(slot.removeFromTop(18).reduced(5, 0));
            labelIdx++;
        }
        comp.setBounds(slot.reduced(5));
        };

    placeCombo(shapeBox);
    placeCombo(materialBox);
    placeCombo(ceilBox);
    placeCombo(wallSBox);
    placeCombo(wallFBBox);
    placeCombo(qualityBox);

    // Phase 182: Place Advanced Button below Quality
    auto advSlot = leftSidebar.removeFromTop(30);
    advancedButton.setBounds(advSlot.reduced(40, 2));

    auto rightArea = area;
    int colWidth = rightArea.getWidth() / 3;
    int rowH = 42;

    auto placeSlider = [&](juce::Slider& s, int col, int row) {
        int x = rightArea.getX() + col * colWidth;
        int y = rightArea.getY() + row * rowH;
        juce::Rectangle<int> r(x, y, colWidth, rowH);

        if (labelIdx < labels.size()) {
            labels[labelIdx]->setBounds(r.removeFromTop(18).reduced(5, 0));
            labelIdx++;
        }
        s.setBounds(r.reduced(5, 0));
        };

    // Column 0
    placeSlider(widthSlider, 0, 0);
    placeSlider(depthSlider, 0, 1);
    placeSlider(heightSlider, 0, 2);
    placeSlider(predelaySlider, 0, 3);
    placeSlider(tempSlider, 0, 4);
    placeSlider(humSlider, 0, 5);
    placeSlider(decaySlider, 0, 6);

    // Column 1
    placeSlider(distSlider, 1, 0);
    placeSlider(panSlider, 1, 1);
    placeSlider(srcHeightSlider, 1, 2);
    placeSlider(modRateSlider, 1, 3);
    placeSlider(modDepthSlider, 1, 4);
    placeSlider(diffSlider, 1, 5);
    placeSlider(densitySlider, 1, 6);

    // Column 2
    placeSlider(absorbSlider, 2, 0);
    placeSlider(driveSlider, 2, 1);
    placeSlider(widthStSlider, 2, 2);
    placeSlider(levelSlider, 2, 3);
    placeSlider(mixSlider, 2, 4);
    placeSlider(dynamicsSlider, 2, 5);
    placeSlider(tiltSlider, 2, 6);

    auto placeFilter = [&](juce::Slider& s, int col) {
        juce::Rectangle<int> r(col * filterW, filterArea.getY(), filterW, 60);
        if (labelIdx < labels.size()) {
            labels[labelIdx]->setBounds(r.removeFromTop(20).reduced(5, 0));
            labelIdx++;
        }
        s.setBounds(r.reduced(5));
        };

    placeFilter(inLCSlider, 0);
    placeFilter(inHCSlider, 1);
    placeFilter(outLCSlider, 2);
    placeFilter(outHCSlider, 3);

    // Center the overlay panel
    if (dynamicsPanel.isVisible()) {
        dynamicsPanel.setBounds(rightArea.getCentreX() - 150, rightArea.getCentreY() - 110, 300, 220);
        dynamicsPanel.toFront(true);
    }
}