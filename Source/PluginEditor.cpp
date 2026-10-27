#include "PluginEditor.h"

namespace Colours
{
    const juce::Colour bg     { 0xff111216 };
    const juce::Colour panel  { 0xff1b1d23 };
    const juce::Colour accent { 0xffff5a1f };   // hot orange
    const juce::Colour accent2{ 0xff8a5cff };   // violet
    const juce::Colour text   { 0xffe8e8ec };
    const juce::Colour dim    { 0xff6c7080 };
}

//==============================================================================
HeavyLookAndFeel::HeavyLookAndFeel()
{
    setColour (juce::Slider::textBoxTextColourId, Colours::text);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::ComboBox::backgroundColourId, Colours::bg);
    setColour (juce::ComboBox::outlineColourId, Colours::dim.withAlpha (0.5f));
    setColour (juce::ComboBox::textColourId, Colours::text);
    setColour (juce::ComboBox::arrowColourId, Colours::accent);
    setColour (juce::PopupMenu::backgroundColourId, Colours::panel);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, Colours::accent);
    setColour (juce::Label::textColourId, Colours::text);
    setColour (juce::ToggleButton::textColourId, Colours::text);
    setColour (juce::ToggleButton::tickColourId, Colours::accent);
    setColour (juce::TextButton::buttonColourId, Colours::bg);
    setColour (juce::TextButton::buttonOnColourId, Colours::accent);
    setColour (juce::TextButton::textColourOffId, Colours::dim);
    setColour (juce::TextButton::textColourOnId, juce::Colours::black);
}

void HeavyLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                                         float startAngle, float endAngle, juce::Slider&)
{
    auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (6.0f);
    const float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto centre = bounds.getCentre();
    const float angle = startAngle + pos * (endAngle - startAngle);
    const float thickness = juce::jmax (3.0f, radius * 0.16f);

    juce::Path track;
    track.addCentredArc (centre.x, centre.y, radius - thickness, radius - thickness, 0.0f, startAngle, endAngle, true);
    g.setColour (Colours::bg);
    g.strokePath (track, juce::PathStrokeType (thickness, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    juce::Path value;
    value.addCentredArc (centre.x, centre.y, radius - thickness, radius - thickness, 0.0f, startAngle, angle, true);
    g.setGradientFill (juce::ColourGradient (Colours::accent2, centre.x - radius, centre.y + radius,
                                             Colours::accent, centre.x + radius, centre.y - radius, false));
    g.strokePath (value, juce::PathStrokeType (thickness, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    const float inner = radius - thickness * 2.2f;
    g.setColour (Colours::panel.brighter (0.15f));
    g.fillEllipse (centre.x - inner, centre.y - inner, inner * 2.0f, inner * 2.0f);

    juce::Path pointer;
    pointer.addRoundedRectangle (-1.5f, -inner, 3.0f, inner * 0.55f, 1.5f);
    g.setColour (Colours::text);
    g.fillPath (pointer, juce::AffineTransform::rotation (angle).translated (centre));
}

//==============================================================================
void GainScope::push (float v)
{
    history[(size_t) writePos] = v;
    writePos = (writePos + 1) % (int) history.size();
}

void GainScope::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (Colours::bg);
    g.fillRoundedRectangle (r, 6.0f);

    const int n = (int) history.size();
    juce::Path fill;
    fill.startNewSubPath (r.getX(), r.getBottom());
    for (int i = 0; i < n; ++i)
    {
        const float v = history[(size_t) ((writePos + i) % n)];
        fill.lineTo (r.getX() + r.getWidth() * (float) i / (float) (n - 1), r.getBottom() - v * (r.getHeight() - 8.0f));
    }
    fill.lineTo (r.getRight(), r.getBottom());
    fill.closeSubPath();

    g.setGradientFill (juce::ColourGradient (Colours::accent.withAlpha (0.85f), 0.0f, r.getY(),
                                             Colours::accent2.withAlpha (0.25f), 0.0f, r.getBottom(), false));
    g.fillPath (fill);

    g.setColour (Colours::dim);
    g.setFont (juce::FontOptions (11.0f));
    g.drawText ("GAIN", r.reduced (8.0f, 4.0f), juce::Justification::topLeft);
}

//==============================================================================
HeavyChopEditor::HeavyChopEditor (HeavyChopProcessor& p)
    : AudioProcessorEditor (&p), processor (p)
{
    setLookAndFeel (&lnf);

    for (int i = 0; i < processor.getNumPrograms(); ++i)
        presetBox.addItem (processor.getProgramName (i), i + 1);
    presetBox.setSelectedId (processor.getCurrentProgram() + 1, juce::dontSendNotification);
    presetBox.onChange = [this] { processor.setCurrentProgram (presetBox.getSelectedId() - 1); };
    addAndMakeVisible (presetBox);

    rateLabel.setJustificationType (juce::Justification::centredRight);
    rateLabel.setColour (juce::Label::textColourId, Colours::dim);
    addAndMakeVisible (rateLabel);

    setupChoice (shape, "shape", "SHAPE");
    setupChoice (rateStart, "rateStart", "RATE START");
    setupChoice (rateEnd, "rateEnd", "RATE END");
    setupChoice (rampBars, "rampBars", "RAMP");
    setupChoice (rampMode, "rampMode", "RAMP MODE");

    setupKnob (rampCurve, "rampCurve", "CURVE");
    setupKnob (depth, "depth", "DEPTH");
    setupKnob (duty, "duty", "GATE LEN");
    setupKnob (smooth, "smooth", "SMOOTH");
    setupKnob (stereo, "stereo", "STEREO");
    setupKnob (patLen, "patLen", "STEPS");

    setupKnob (drive, "drive", "DRIVE");
    setupKnob (squash, "squash", "SQUASH");
    setupKnob (weight, "weight", "WEIGHT");
    setupKnob (heavyMix, "heavyMix", "HEAVY MIX");
    setupKnob (width, "width", "WIDTH");
    setupKnob (output, "output", "OUTPUT");

    for (int i = 0; i < 16; ++i)
    {
        steps[i].setClickingTogglesState (true);
        steps[i].setButtonText (juce::String (i + 1));
        addAndMakeVisible (steps[i]);
        stepAttachments[i] = std::make_unique<ButtonAttachment> (processor.apvts, "step" + juce::String (i + 1), steps[i]);
    }

    addAndMakeVisible (clip);
    clipAttachment = std::make_unique<ButtonAttachment> (processor.apvts, "clip", clip);

    addAndMakeVisible (scope);

    setSize (960, 580);
    startTimerHz (60);
}

HeavyChopEditor::~HeavyChopEditor()
{
    setLookAndFeel (nullptr);
}

void HeavyChopEditor::setupKnob (Knob& k, const juce::String& paramId, const juce::String& name)
{
    k.slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    k.slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 80, 18);
    addAndMakeVisible (k.slider);
    k.label.setText (name, juce::dontSendNotification);
    k.label.setJustificationType (juce::Justification::centred);
    k.label.setFont (juce::FontOptions (12.0f, juce::Font::bold));
    k.label.setColour (juce::Label::textColourId, Colours::dim);
    addAndMakeVisible (k.label);
    k.attachment = std::make_unique<SliderAttachment> (processor.apvts, paramId, k.slider);
}

void HeavyChopEditor::setupChoice (Choice& c, const juce::String& paramId, const juce::String& name)
{
    if (auto* param = dynamic_cast<juce::AudioParameterChoice*> (processor.apvts.getParameter (paramId)))
        c.box.addItemList (param->choices, 1);
    addAndMakeVisible (c.box);
    c.label.setText (name, juce::dontSendNotification);
    c.label.setFont (juce::FontOptions (11.0f, juce::Font::bold));
    c.label.setColour (juce::Label::textColourId, Colours::dim);
    addAndMakeVisible (c.label);
    c.attachment = std::make_unique<ComboAttachment> (processor.apvts, paramId, c.box);
}

void HeavyChopEditor::timerCallback()
{
    scope.push (processor.displayGain.load());
    scope.repaint();

    const int step = processor.displayStep.load();
    const bool patternMode = (int) processor.apvts.getRawParameterValue ("shape")->load() == ModEngine::Pattern;
    const int len = (int) processor.apvts.getRawParameterValue ("patLen")->load();

    for (int i = 0; i < 16; ++i)
    {
        const bool active = i < len;
        steps[i].setAlpha (active ? (patternMode ? 1.0f : 0.45f) : 0.15f);
        steps[i].setColour (juce::TextButton::buttonColourId,
                            (patternMode && i == step) ? Colours::accent2.withAlpha (0.6f) : Colours::bg);
    }
    lastStep = step;

    // show current rate as a note value, e.g. "1/12.7" while ramping
    const float rate = processor.displayRate.load();
    rateLabel.setText ("rate  1/" + juce::String (rate * 4.0f, 1) + " note", juce::dontSendNotification);
}

void HeavyChopEditor::paint (juce::Graphics& g)
{
    g.fillAll (Colours::bg);

    g.setColour (Colours::panel);
    g.fillRoundedRectangle (motionArea.toFloat(), 10.0f);
    g.fillRoundedRectangle (heavyArea.toFloat(), 10.0f);

    g.setColour (Colours::text);
    g.setFont (juce::FontOptions (26.0f, juce::Font::bold));
    g.drawText ("HEAVY", 20, 12, 100, 32, juce::Justification::centredLeft);
    g.setColour (Colours::accent);
    g.drawText ("CHOP", 108, 12, 100, 32, juce::Justification::centredLeft);

    g.setFont (juce::FontOptions (13.0f, juce::Font::bold));
    g.setColour (Colours::accent);
    g.drawText ("MOTION", motionArea.getX() + 14, motionArea.getY() + 8, 200, 18, juce::Justification::centredLeft);
    g.drawText ("WEIGHT", heavyArea.getX() + 14, heavyArea.getY() + 8, 200, 18, juce::Justification::centredLeft);

    g.setColour (Colours::dim);
    g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
    g.drawText ("CHOP PATTERN  (Shape = Pattern)", patternArea.getX(), patternArea.getY() - 18, 300, 16, juce::Justification::centredLeft);
}

void HeavyChopEditor::resized()
{
    auto area = getLocalBounds().reduced (14);
    auto header = area.removeFromTop (44);
    presetBox.setBounds (header.removeFromRight (260).withSizeKeepingCentre (260, 28));
    rateLabel.setBounds (header.removeFromRight (200));
    area.removeFromTop (6);

    heavyArea = area.removeFromRight (300);
    area.removeFromRight (12);
    motionArea = area;

    // ----- motion panel -----
    auto m = motionArea.reduced (14);
    m.removeFromTop (24);

    auto comboRow = m.removeFromTop (48);
    Choice* choices[] = { &shape, &rateStart, &rateEnd, &rampBars, &rampMode };
    const int cw = comboRow.getWidth() / 5;
    for (auto* c : choices)
    {
        auto cell = comboRow.removeFromLeft (cw).reduced (4, 0);
        c->label.setBounds (cell.removeFromTop (18));
        c->box.setBounds (cell.removeFromTop (26));
    }

    m.removeFromTop (10);
    auto knobRow = m.removeFromTop (120);
    Knob* motionKnobs[] = { &rampCurve, &depth, &duty, &smooth, &stereo, &patLen };
    const int kw = knobRow.getWidth() / 6;
    for (auto* k : motionKnobs)
    {
        auto cell = knobRow.removeFromLeft (kw);
        k->label.setBounds (cell.removeFromTop (18));
        k->slider.setBounds (cell);
    }

    m.removeFromTop (28);
    patternArea = m.removeFromTop (40);
    auto stepRow = patternArea;
    const int sw = stepRow.getWidth() / 16;
    for (int i = 0; i < 16; ++i)
        steps[i].setBounds (stepRow.removeFromLeft (sw).reduced (2, 0));

    m.removeFromTop (14);
    scope.setBounds (m);

    // ----- heavy panel -----
    auto h = heavyArea.reduced (14);
    h.removeFromTop (28);
    clip.setBounds (h.removeFromBottom (28));
    Knob* heavyKnobs[] = { &drive, &squash, &weight, &heavyMix, &width, &output };
    const int rowH = h.getHeight() / 3;
    for (int row = 0; row < 3; ++row)
    {
        auto r = h.removeFromTop (rowH);
        for (int col = 0; col < 2; ++col)
        {
            auto* k = heavyKnobs[row * 2 + col];
            auto cell = r.removeFromLeft (h.getWidth() / 2);
            k->label.setBounds (cell.removeFromTop (18));
            k->slider.setBounds (cell);
        }
    }
}
