#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <cmath>

#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "KnobDesign.h"

static int parseInt(const char* s, int fallback)
{
    if (s == nullptr) return fallback;
    const auto str = juce::String(s).trim();
    return str.isEmpty() ? fallback : str.getIntValue();
}

static float parseFloat(const char* s, float fallback)
{
    if (s == nullptr) return fallback;
    const auto str = juce::String(s).trim();
    return str.isEmpty() ? fallback : static_cast<float>(str.getDoubleValue());
}

int main(int argc, char** argv)
{
    if (argc < 2)
    {
        std::cerr << "usage: " << argv[0] << " <output.png> [width] [height] [scale]\n";
        return 1;
    }

    const juce::File outFile{ juce::String::fromUTF8(argv[1]) };
    const int   width  = parseInt (argc > 2 ? argv[2] : nullptr, KnobDesign::defaultWidth);
    const int   height = parseInt (argc > 3 ? argv[3] : nullptr, KnobDesign::defaultHeight);
    const float scale  = parseFloat(argc > 4 ? argv[4] : nullptr, 2.0f);

    juce::ScopedJuceInitialiser_GUI scopedGUI;

    QuirkAudioProcessor processor;
    const double sampleRate = 44100.0;
    const int blockSize = 512;
    processor.prepareToPlay(sampleRate, blockSize);

    {
        juce::AudioBuffer<float> buf(2, blockSize);
        juce::MidiBuffer midi;

        auto noteOn = juce::MidiMessage::noteOn(1, 60, 0.8f);
        noteOn.setTimeStamp(0);
        midi.addEvent(noteOn, 0);

        buf.clear();
        processor.processBlock(buf, midi);

        midi.clear();
        for (int i = 0; i < 10; ++i)
        {
            buf.clear();
            processor.processBlock(buf, midi);
        }
    }

    std::unique_ptr<juce::AudioProcessorEditor> editor{ processor.createEditor() };
    if (editor == nullptr)
    {
        std::cerr << "error: createEditor() returned null\n";
        return 2;
    }
    editor->setSize(width, height);

    if (auto* qEditor = dynamic_cast<QuirkAudioProcessorEditor*>(editor.get()))
        qEditor->setChromeVisible(false);

    auto snap = editor->createComponentSnapshot(editor->getLocalBounds(), false, scale);

    {
        const float widthScale = static_cast<float>(width) / static_cast<float>(KnobDesign::defaultWidth);
        const int insetPx = static_cast<int>(20.0f * widthScale * scale);
        const float cornerRadius = 70.0f * widthScale * scale;

        const int outW = snap.getWidth() - 2 * insetPx;
        const int outH = snap.getHeight() - 2 * insetPx;

        juce::Image framed{ juce::Image::ARGB, outW, outH, true };
        juce::Graphics rg{ framed };

        juce::Path mask;
        mask.addRoundedRectangle(0.0f, 0.0f,
                                 static_cast<float>(outW),
                                 static_cast<float>(outH),
                                 cornerRadius);
        rg.reduceClipRegion(mask);
        rg.drawImageAt(snap, -insetPx, -insetPx);

        snap = framed;
    }

    outFile.deleteFile();
    juce::FileOutputStream stream{ outFile };
    if (!stream.openedOk())
    {
        std::cerr << "error: cannot write to " << outFile.getFullPathName() << "\n";
        return 3;
    }

    juce::PNGImageFormat png;
    if (!png.writeImageToStream(snap, stream))
    {
        std::cerr << "error: PNG encode failed\n";
        return 4;
    }

    std::cout << "wrote " << outFile.getFullPathName()
              << " (" << snap.getWidth() << "x" << snap.getHeight() << ")\n";
    return 0;
}
