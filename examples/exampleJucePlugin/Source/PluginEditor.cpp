#include "PluginProcessor.h"
#include "PluginEditor.h"

#include "gmpi_ui_demo_text.h"

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

void BouncingBoxesComponent::onRender(gmpi::drawing::Graphics& g)
{
	drawTextDemo(g, { 400, 300 });


}
