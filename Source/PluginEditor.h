/*
  ==============================================================================
    PluginEditor.h
    Phase 182: Advanced Dynamics UI (Overlay Fixed)

    FIXES:
    - Removed 'juce::CalloutBox' to resolve compiler errors.
    - Implemented DynamicsPanel as a direct child component (Overlay).
  ==============================================================================
*/
#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "FDN_DSP.h"
#include <map>

// --- Custom LookAndFeel ---
class AbletonLightLookAndFeel : public juce::LookAndFeel_V4 {
public:
    AbletonLightLookAndFeel() {
        setColour(juce::ResizableWindow::backgroundColourId, juce::Colour(0xff303030));
        setColour(juce::Slider::thumbColourId, juce::Colour(0xffffa500));
        setColour(juce::Slider::trackColourId, juce::Colour(0xff505050));
        setColour(juce::Slider::backgroundColourId, juce::Colour(0xff202020));
        setColour(juce::Label::textColourId, juce::Colours::white);
        setColour(juce::Label::outlineColourId, juce::Colours::transparentBlack);
        setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xff202020));
        setColour(juce::ComboBox::outlineColourId, juce::Colour(0xff606060));
        setColour(juce::ComboBox::textColourId, juce::Colours::white);
        setColour(juce::ComboBox::arrowColourId, juce::Colour(0xffffa500));
        setColour(juce::PopupMenu::backgroundColourId, juce::Colour(0xff252525));
        setColour(juce::PopupMenu::textColourId, juce::Colours::white);
        setColour(juce::PopupMenu::highlightedBackgroundColourId, juce::Colour(0xffffa500));
        setColour(juce::TextButton::buttonColourId, juce::Colour(0xff404040));
        setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    }

    void getIdealPopupMenuItemSize(const juce::String& text, bool isSeparator, int standardMenuItemHeight, int& idealWidth, int& idealHeight) override {
        if (isSeparator) { idealWidth = 50; idealHeight = 10; }
        else {
            juce::Font font = getPopupMenuFont();
            idealHeight = 24;
            idealWidth = font.getStringWidth(text) + 32;
        }
    }
    juce::Font getPopupMenuFont() override { return juce::Font("Arial", 15.0f, juce::Font::plain); }
    juce::Font getLabelFont(juce::Label&) override { return juce::Font("Arial", 15.0f, juce::Font::bold); }

    void drawLinearSlider(juce::Graphics& g, int x, int y, int width, int height,
        float sliderPos, float minSliderPos, float maxSliderPos,
        const juce::Slider::SliderStyle, juce::Slider& slider) override
    {
        float trackHeight = 14.0f;
        auto trackRect = juce::Rectangle<float>((float)x, (float)y + (float)height * 0.5f - trackHeight * 0.5f, (float)width, trackHeight);
        g.setColour(findColour(juce::Slider::backgroundColourId));
        g.fillRect(trackRect);
        g.setColour(juce::Colour(0xff606060));
        g.drawRect(trackRect, 1.0f);
        g.setColour(juce::Colour(0xffffa500));
        float fillW = sliderPos - (float)x;
        g.fillRect(trackRect.getX(), trackRect.getY(), fillW, trackRect.getHeight());
    }

    void drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height, float sliderPos,
        const float rotaryStartAngle, const float rotaryEndAngle, juce::Slider& slider) override
    {
        auto radius = (float)juce::jmin(width / 2, height / 2) - 4.0f;
        auto centreX = (float)x + (float)width * 0.5f;
        auto centreY = (float)y + (float)height * 0.5f;
        auto rx = centreX - radius;
        auto ry = centreY - radius;
        auto rw = radius * 2.0f;
        auto angle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);
        g.setColour(findColour(juce::Slider::backgroundColourId));
        g.fillEllipse(rx, ry, rw, rw);
        g.setColour(findColour(juce::Slider::trackColourId));
        g.drawEllipse(rx, ry, rw, rw, 1.0f);
        juce::Path p;
        auto pointerLength = radius * 0.7f;
        auto pointerThickness = 3.0f;
        p.addRectangle(-pointerThickness * 0.5f, -radius, pointerThickness, pointerLength);
        p.applyTransform(juce::AffineTransform::rotation(angle).translated(centreX, centreY));
        g.setColour(findColour(juce::Slider::thumbColourId));
        g.fillPath(p);
    }
};

class InfoBar : public juce::Component {
    juce::String text = "Ready.";
public:
    InfoBar() {}
    void showInfo(const juce::String& t) { text = t; repaint(); }
    void paint(juce::Graphics& g) override {
        g.fillAll(juce::Colour(0xff252525));
        g.setColour(juce::Colours::white);
        g.setFont(juce::Font("Arial", 20.0f, juce::Font::plain));
        g.drawFittedText(text, getLocalBounds().reduced(15, 5), juce::Justification::centredLeft, 2);
    }
};

class AbsorptionGraph : public juce::Component, public juce::Timer {
public:
    AbsorptionGraph(FdnReverbAudioProcessor& p) : processor(p) { startTimerHz(30); }
    void timerCallback() override;
    void paint(juce::Graphics& g) override;
private:
    FdnReverbAudioProcessor& processor;
};

// --- Advanced Dynamics Panel Class Definition ---
class DynamicsPanel : public juce::Component {
public:
    DynamicsPanel(FdnReverbAudioProcessor& p);
    ~DynamicsPanel() override;
    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    FdnReverbAudioProcessor& processor;
    juce::Slider threshSlider, ratioSlider, attackSlider, releaseSlider;
    std::vector<std::unique_ptr<juce::Label>> labels;
    using Attachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    std::vector<std::unique_ptr<Attachment>> attachments;

    void setupSlider(juce::Slider& s, const juce::String& paramID, const juce::String& name, const juce::String& suffix);
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DynamicsPanel)
};

struct Point3D { float x, y, z; };

class RoomVisualizer : public juce::Component, public juce::Timer {
public:
    RoomVisualizer(FdnReverbAudioProcessor& p) : processor(p) { startTimerHz(30); }
    void timerCallback() override;
    void paint(juce::Graphics& g) override;
private:
    FdnReverbAudioProcessor& processor;
};

class FdnReverbAudioProcessorEditor : public juce::AudioProcessorEditor, public juce::Timer, public juce::MouseListener
{
public:
    FdnReverbAudioProcessorEditor(FdnReverbAudioProcessor& p);
    ~FdnReverbAudioProcessorEditor() override;
    void paint(juce::Graphics&) override;
    void resized() override;
    void timerCallback() override;

    void mouseEnter(const juce::MouseEvent& event) override;
    void mouseExit(const juce::MouseEvent& event) override;

private:
    FdnReverbAudioProcessor& audioProcessor;
    AbletonLightLookAndFeel lightLookAndFeel;

    InfoBar infoBar;
    RoomVisualizer roomVis;
    AbsorptionGraph absorptionGraph;

    // The Overlay Panel
    DynamicsPanel dynamicsPanel;

    juce::TextButton presetButton;
    juce::TextButton saveButton;
    juce::TextButton panicButton;
    juce::TextButton advancedButton;

    juce::Label rt60Label;

    juce::Slider widthSlider, depthSlider, heightSlider;
    juce::Slider absorbSlider, tempSlider, humSlider;
    juce::Slider modRateSlider, modDepthSlider, diffSlider;
    juce::Slider distSlider, panSlider, srcHeightSlider;
    juce::Slider widthStSlider, levelSlider, mixSlider;
    juce::Slider predelaySlider;

    juce::Slider driveSlider, densitySlider;
    juce::Slider decaySlider;

    juce::Slider dynamicsSlider;
    juce::Slider tiltSlider;

    juce::ComboBox qualityBox;
    juce::Label qualityLabel;

    juce::Slider inLCSlider, inHCSlider, outLCSlider, outHCSlider;

    juce::ComboBox materialBox, ceilBox, wallSBox, wallFBBox, shapeBox;

    juce::ImageButton cubeButton;
    juce::Image logoImage;

    juce::PopupMenu factoryPresetMenu;
    juce::PopupMenu userPresetMenu;
    void buildFactoryPresetMenu();
    void buildUserPresetMenu();

    std::vector<std::unique_ptr<juce::Label>> labels;

    using Attachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ComboAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    std::vector<std::unique_ptr<Attachment>> sliderAtts;
    std::vector<std::unique_ptr<ComboAttachment>> comboAtts;

    std::map<juce::Component*, juce::String> helpTexts;

    void setupSlider(juce::Slider& s, const juce::String& paramID, const juce::String& desc);
    void setupCombo(juce::ComboBox& box, const juce::String& paramID, const juce::String& desc, const std::vector<int>& indices = {});
    void showPresetMenu();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FdnReverbAudioProcessorEditor)
};