#include "PluginProcessor.h"
#include "PluginEditor.h"

#include "gmpi_ui_demo_text.h"
#include "gmpi_ui_demo_lines.h"

//==============================================================================
PluginEditor::PluginEditor (NewProjectAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p)
{
	addAndMakeVisible(clientComponent);
    setSize (400, 300);
}

void GmpiDrawingDemoComponent::onRender(gmpi::drawing::Graphics& g)
{
	// draw the current demo, either shapes, or text.
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

void PluginEditor::resized()
{
	// when the parent component is resized, we also resize the GMPI component.
	clientComponent.setBounds(getBounds());
}

gmpi::ReturnCode GmpiDrawingDemoComponent::onPointerUp(gmpi::drawing::Point point, int32_t pflags)
{
	// when clicked, increment the current demo, and repaint the component.
	demo_idx = (demo_idx + 1) % 2;
	invalidateRect(); // GMPI-UI equivalent of 'repaint()'

	return gmpi::ReturnCode::Handled;
}
