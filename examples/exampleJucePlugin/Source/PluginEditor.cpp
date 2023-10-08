#include "PluginProcessor.h"
#include "PluginEditor.h"

#include "gmpi_ui_demo_text.h"
#include "gmpi_ui_demo_lines.h"

//==============================================================================
PluginEditor::PluginEditor (NewProjectAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p)
{
	//	addAndMakeVisible(boxesComponent);
	addAndMakeVisible(clientComponent);

    setSize (400, 300);
//	setInterceptsMouseClicks(true, true);
}

void PluginEditor::resized()
{
	//	boxesComponent.setBounds(getBounds());
	clientComponent.setBounds(getBounds());
}

void PluginEditor::mouseDown(const juce::MouseEvent& e)
{
//	nextDemo();
//	int x = 9;
}

void GmpiDrawingDemoComponent::mouseDown(const juce::MouseEvent& e)
{
	nextDemo();
}

void GmpiDrawingDemoComponent::mouseUp(const juce::MouseEvent& e)
{
	nextDemo();
}

void GmpiDrawingDemoComponent::onRender(gmpi::drawing::Graphics& g)
{
	switch (demo_idx)
	{
	case 0:
		drawTextDemo(g, { 400, 300 });
		break;
	case 1:
		drawLinesDemo(g, { 400, 300 });
		break;
	}
}

void GmpiDrawingDemoComponent::nextDemo()
{
	demo_idx = (demo_idx + 1) % 2;

	invalidateRect(); // GMPI-UI equivalent of 'repaint()'
}