#include "SE2JUCE_Processor.h"
#include "SE2JUCE_Editor.h"

#include "unicode_conversion.h"
#include "RawConversions.h"

//==============================================================================
SE2JUCE_Processor::SE2JUCE_Processor() :
    // init the midi converter
    midiConverter(
        // provide a lambda to accept converted MIDI 2.0 messages
        [this](const gmpi::midi::message_view& msg, int offset)
        {
            processor.MidiIn(offset, msg.begin(), static_cast<int>(msg.size()));
        }
    ),

#ifndef JucePlugin_PreferredChannelConfigurations
     AudioProcessor (BusesProperties()
                     #if ! JucePlugin_IsMidiEffect
                      #if ! JucePlugin_IsSynth
                       .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                      #endif
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                     #endif
                       )
#endif
{
    processor.connectPeer(&controller);
    controller.Initialize(this);

    for(auto& itp : controller.nativeParameters())
    {
        auto p = itp.second;

//        _RPTW2(0, L"%2d: %s\n", index, p->name_.c_str());

        juce::AudioProcessorParameter* juceParameter = {};
        if (p->isEnum())
        {
            const int defaultItemIndex = {};

            it_enum_list it( (std::wstring)(p->getValueRaw(gmpi::MP_FT_ENUM_LIST, 0)) );
            juce::StringArray choices;
            for (it.First(); !it.IsDone(); it.Next())
            {
                auto e = it.CurrentItem();
                choices.add(juce::String(JmUnicodeConversions::WStringToUtf8(e->text)));
            }

            juceParameter =
                new juce::AudioParameterChoice(
                    {std::to_string(p->getNativeIndex()), 1},       // parameterID/versionhint
                    WStringToUtf8(p->name_).c_str(),                // parameter name
                    choices,
                    defaultItemIndex
                );
        }
        else
        {
            juceParameter =
                new juce::AudioParameterFloat(
                    {std::to_string(p->getNativeIndex()), 1},       // parameterID/versionhint
                    WStringToUtf8(p->name_).c_str(),                // parameter name
                    static_cast<float>(p->normalisedToReal(0.0)),   // minimum value
                    static_cast<float>(p->normalisedToReal(1.0)),   // maximum value
                    static_cast<float>(p->getValueReal())           // default value
                );
        }

        addParameter(juceParameter);

        juceParameter->addListener(this);
    }
}

// Ableton: called on main thread when manipulating param from generic panel.
// Cubase: called on audio thread when automating param.
// Cubase: called on main thread when changing param via mouse.
void SE2JUCE_Processor::parameterValueChanged(int parameterIndex, float newValue)
{
    controller.setParameterNormalizedUnsafe(parameterIndex, newValue);
}

void SE2JUCE_Processor::parameterGestureChanged(int parameterIndex, bool gestureIsStarting)
{
    if (auto p = controller.getDawParameter(parameterIndex); p)
    {
        p->setGrabbed(gestureIsStarting);
    }
}

SE2JUCE_Processor::~SE2JUCE_Processor()
{
}

//==============================================================================
const juce::String SE2JUCE_Processor::getName() const
{
    return JucePlugin_Name;
}

bool SE2JUCE_Processor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool SE2JUCE_Processor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool SE2JUCE_Processor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double SE2JUCE_Processor::getTailLengthSeconds() const
{
    return 0.0;
}

int SE2JUCE_Processor::getNumPrograms()
{
    return controller.getPresetCount();   // NB: some hosts don't cope very well if you tell them there are 0 programs,
                // so this should be at least 1, even if you're not really implementing programs.
}

int SE2JUCE_Processor::getCurrentProgram()
{
    const auto parameterHandle = controller.getParameterHandle(-1, -1 - HC_PROGRAM);

    const auto program = (int32_t) controller.getParameterValue(parameterHandle, gmpi::MP_FT_VALUE);
    _RPT1(0, "getCurrentProgram() -> %d\n", program);
    return program;
}

void SE2JUCE_Processor::setCurrentProgram (int index)
{
    controller.loadFactoryPreset(index, true);
}

const juce::String SE2JUCE_Processor::getProgramName (int index)
{
    const auto safeIndex = (std::min)(controller.getPresetCount(), (std::max)(0, index));
    return { controller.getPresetInfo(safeIndex).name};
}

void SE2JUCE_Processor::changeProgramName (int, const juce::String&)
{
}

//==============================================================================
void SE2JUCE_Processor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    timeInfo.resetToDefault();
    memset(&timeInfoSe, 0, sizeof(timeInfoSe));
    timeInfoSe.tempo = 120.0;
    timeInfoSe.sampleRate = sampleRate;
    timeInfoSe.timeSigDenominator = timeInfoSe.timeSigNumerator = 4;

    processor.prepareToPlay(
        this,
        static_cast<int32_t>(sampleRate),
        samplesPerBlock,
        !isNonRealtime()
    );
}

void SE2JUCE_Processor::releaseResources()
{
    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc.
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool SE2JUCE_Processor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
  #if JucePlugin_IsMidiEffect
    juce::ignoreUnused (layouts);
    return true;
  #else
    // This is the place where you check if the layout is supported.
    // In this template code we only support mono or stereo.
    // Some plugin hosts, such as certain GarageBand versions, will only
    // load plugins that support stereo bus layouts.
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    // This checks if the input layout matches the output layout
   #if ! JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
   #endif

    return true;
  #endif
}
#endif

void SE2JUCE_Processor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;

    if (getPlayHead() && getPlayHead()->getCurrentPosition(timeInfo))
    {
        constexpr double nanoSecondsToSeconds = 1.0f / 1000000000.f;

        timeInfoSe.tempo            = timeInfo.bpm;
        timeInfoSe.barStartPos      = timeInfo.ppqPositionOfLastBarStart;
        timeInfoSe.cycleEndPos      = timeInfo.ppqLoopEnd;
        timeInfoSe.cycleStartPos    = timeInfo.ppqLoopStart;
        timeInfoSe.nanoSeconds      = timeInfo.timeInSeconds * nanoSecondsToSeconds;
        timeInfoSe.ppqPos           = timeInfo.ppqPosition;
        timeInfoSe.samplePos        = static_cast<double>(timeInfo.timeInSamples);
        // timeInfoSe.samplesToNextClock = timeInfo.;
        timeInfoSe.timeSigDenominator = timeInfo.timeSigDenominator;
        timeInfoSe.timeSigNumerator = timeInfo.timeSigNumerator;

        timeInfoSe.flags = my_VstTimeInfo::kVstNanosValid | my_VstTimeInfo::kVstPpqPosValid | my_VstTimeInfo::kVstTempoValid;
        if (timeInfo.isPlaying)
        {
            timeInfoSe.flags |= my_VstTimeInfo::kVstTransportPlaying;
        }
        if (timeInfo.isLooping)
        {
            timeInfoSe.flags |= my_VstTimeInfo::kVstTransportCycleActive;
        }
    }

    processor.UpdateTempo(&timeInfoSe);

#ifdef _DEBUG
    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();
    // In case we have more outputs than inputs, this code clears any output
    // channels that didn't contain input data, (because these aren't
    // guaranteed to be empty - they may contain garbage).
    // This is here to avoid people getting screaming feedback
    // when they first compile a plugin, but obviously you don't need to keep
    // this code if your algorithm always overwrites all the output channels.
    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());
#endif

    for (const auto msg : midiMessages)
    {
        midiConverter.processMidi({ msg.data, msg.numBytes }, msg.samplePosition);
    }

    processor.process(
        buffer.getNumSamples(),
        buffer.getArrayOfReadPointers(),
        buffer.getArrayOfWritePointers(),
        getTotalNumInputChannels(),
        getTotalNumOutputChannels()
    );
}

//==============================================================================
bool SE2JUCE_Processor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
}

juce::AudioProcessorEditor* SE2JUCE_Processor::createEditor()
{
    return new SynthEditEditor(*this, controller);
}

//==============================================================================
void SE2JUCE_Processor::getStateInformation (juce::MemoryBlock& destData)
{
    // You should use this method to store your parameters in the memory block.
    // You could do that either as raw data, or use the XML or ValueTree classes
    // as intermediaries to make it easy to save and load complex data.

    const bool active = true; // guess. ??
    std::string chunk;
    processor.getPresetState(chunk, active);

    destData.replaceAll(chunk.data(), chunk.size());
}

void SE2JUCE_Processor::setStateInformation (const void* data, int sizeInBytes)
{
    const std::string chunk(static_cast<const char*>(data), sizeInBytes);
	controller.setPresetFromDaw(chunk, true);
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SE2JUCE_Processor();
}
