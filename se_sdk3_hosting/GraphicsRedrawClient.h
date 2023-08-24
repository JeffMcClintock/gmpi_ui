//
//  GraphicsRedrawClient.h
//  Switches
//
//  Created by Jeff McClintock on 4/10/22.
//

#ifndef GraphicsRedrawClient_h
#define GraphicsRedrawClient_h


// notify a plugin when it's time to display a new frame.
// Supports the client optimizing how often it checks the DSP queue
class DECLSPEC_NOVTABLE IGraphicsRedrawClient : public gmpi::IMpUnknown
{
public:
	virtual void PreGraphicsRedraw() = 0;

	// {4CCF9E3A-05AE-46C8-AEBB-1FFC5E950494}
	inline static const gmpi::MpGuid guid =
	{ 0x4ccf9e3a, 0x5ae, 0x46c8, { 0xae, 0xbb, 0x1f, 0xfc, 0x5e, 0x95, 0x4, 0x94 } };
};

#endif /* GraphicsRedrawClient_h */
