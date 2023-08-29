#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
PluginEditor::PluginEditor (NewProjectAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p)
{
	addAndMakeVisible(boxesComponent);

    setSize (400, 300);
}

void PluginEditor::resized()
{
	boxesComponent.setBounds(getBounds());
}

