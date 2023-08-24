/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "SE2JUCE_Processor.h"
#include "SE2JUCE_Controller.h"
#include "SynthRuntime.h"
#include "mp_midi.h"

//==============================================================================
/**
*/
class SE2JUCE_Processor : public juce::AudioProcessor, public juce::AudioProcessorParameter::Listener, public IShellServices
{
    SynthRuntime processor;

    // unusual: Controller is held as member of processor, as JUCE don't support the concept.
    SeJuceController controller;
    juce::AudioPlayHead::CurrentPositionInfo timeInfo;  // JUCE format
    my_VstTimeInfo timeInfoSe;                          // SE format
    gmpi::midi_2_0::MidiConverter2 midiConverter;

public:
    //==============================================================================
    SE2JUCE_Processor();
    ~SE2JUCE_Processor() override;

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

   #ifndef JucePlugin_PreferredChannelConfigurations
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
   #endif

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    //==============================================================================
    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    //==============================================================================
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    //==============================================================================
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    //===IShellServices=============================================================
    void onQueDataAvailable() override {}
    void flushPendingParameterUpdates() override {}

    //===AudioProcessorParameter::Listener==========================================
    void parameterValueChanged(int parameterIndex, float newValue) override;
    void parameterGestureChanged(int parameterIndex, bool gestureIsStarting) override;

private:
    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SE2JUCE_Processor)
};
