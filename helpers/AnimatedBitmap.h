#pragma once
#include <algorithm>
#include "../shared/ImageMetadata.h"
#include "./ImageCache.h"

//#ifdef _DEBUG
//#include <iostream>
//#endif
namespace gmpi_helper
{

class AnimatedBitmap : public ImageCacheClient
{
protected:
	gmpi::drawing::Bitmap bitmap_;
	ImageMetadata* bitmapMetadata_{};

	gmpi::drawing::Point m_ptPrev;
//	bool smallDragSuppression;
	float m_rot_mode;
	int drawAt;
	bool hitTestPixelAccurate;

	// Rotary switch mode.
	int m_rotary_steps;
	float m_rotary_switch_start_angle; // from 0.0 (3oclock) to 1.0
	float m_rotary_switch_range; // from 0.0 to 1.0 (360 degrees)
	int last_frame_idx;
	int rotary_current_step = 0;

	// Uses co-ords relative to widget (needs offset applied before use) 
	bool bitmapHitTestLocal(GmpiDrawing_API::MP1_POINT point)
	{
		if (bitmapMetadata_ == nullptr || !gmpi::drawing::AccessPtr::get(bitmap_))
			return false;

		// Hit Testing.
		gmpi::drawing::Rect bmRect;
		gmpi::drawing::Size bmOffset{};

		switch (bitmapMetadata_->mode)
		{
		case ABM_ANIMATED:
		default:
		{
			//			_RPT2(_CRT_WARN, "knob Blits to 0,0, W=%d,H=%d\n", s.cx, s.cy);
			bmOffset.width = (float)bitmapMetadata_->padding_left;
			bmOffset.height = (float)bitmapMetadata_->padding_top;

			bmRect.left = 0;
			bmRect.right = bmRect.left + bitmapMetadata_->frameSize.width;
			bmRect.top = static_cast<float>(drawAt);
			bmRect.bottom = bmRect.top + bitmapMetadata_->frameSize.height;
		}
		break;

		case ABM_SLIDER:
		{
			if (!hitTestPixelAccurate)
			{
				gmpi::drawing::Rect backgroundRect{ 0, 0, (float)bitmapMetadata_->frameSize.width, (float)bitmapMetadata_->frameSize.height };
				return !(point.x < backgroundRect.left || point.x > backgroundRect.right || point.y < backgroundRect.top || point.y > backgroundRect.bottom);
			}

			// draw slider knob
			if (bitmapMetadata_->orientation == 0) // bitmap->GetMouseMode() == 0) // vertical
			{
				//int drawAt = (int) (bitmapMetadata_->handle_range_hi - ( animationPosition * ( bitmapMetadata_->handle_range_hi - bitmapMetadata_->handle_range_lo ) ));
				// fix for 'hamond' vertical draw bars
				// clip drawing to top of control
				float draw_pos = (float)drawAt;
				float source_y = bitmapMetadata_->handle_rect.top;
				float dest_height = getHeight(bitmapMetadata_->handle_rect);

				if (draw_pos < 0)
				{
					float chop_off_top = -draw_pos;
					draw_pos = 0;
					dest_height -= chop_off_top;
					source_y += chop_off_top;
				}

				// scale pos (0-100) by knob range
				//				bitmap->TransparentBlit2(*pDC, draw_p.x + knob_rect.left, draw_p.y + draw_pos, knob_rect.Width(), dest_height, knob_rect.left, source_y);
				//gmpi::drawing::Rect knob_rect(bitmapMetadata_->handle_rect.left, source_y, bitmapMetadata_->handle_rect.right, bitmapMetadata_->handle_rect.bottom);
				//gmpi::drawing::Rect dest_rect(bitmapMetadata_->handle_rect.left, draw_pos, bitmapMetadata_->handle_rect.right, draw_pos + dest_height);
				//dc->DrawBitmap(bitmap, &dest_rect, 1.0f, 1, &knob_rect);

				bmRect.left = bitmapMetadata_->handle_rect.left;
				bmRect.right = bitmapMetadata_->handle_rect.right;
				bmRect.top = source_y;
				bmRect.bottom = bmRect.top + dest_height;

				bmOffset.height = draw_pos;
			}
			else
			{
				// TODO, proper clipping on all sides !!!
				// fix for horizontal draw bars
				// clip drawing to left of control
				float draw_pos = (float)drawAt;
				float source_x = bitmapMetadata_->handle_rect.left;
				float dest_width = getWidth(bitmapMetadata_->handle_rect);

				if (draw_pos < 0)
				{
					float chop_off_left = -draw_pos;
					draw_pos = 0;
					dest_width -= chop_off_left;
					source_x += chop_off_left;
				}

				// scale pos (0-100) by knob range
				//				bitmap->TransparentBlit2(*pDC, draw_p.x + draw_pos, draw_p.y + knob_rect.top, dest_width, knob_rect.Height(), source_x, knob_rect.top);
				//gmpi::drawing::Rect knob_rect(source_x, bitmapMetadata_->handle_rect.top, bitmapMetadata_->handle_rect.right, bitmapMetadata_->handle_rect.bottom);
				//gmpi::drawing::Rect dest_rect(draw_pos, bitmapMetadata_->handle_rect.top, draw_pos + dest_width, bitmapMetadata_->handle_rect.bottom);
				//dc->DrawBitmap(bitmap, &dest_rect, 1.0f, 1, &knob_rect);

				bmRect.left = source_x;
				bmRect.right = bmRect.left + dest_width;
				bmRect.top = bitmapMetadata_->handle_rect.top;
				bmRect.bottom = bitmapMetadata_->handle_rect.bottom;

				bmOffset.width = draw_pos;
			}
		}
		break;

		case ABM_BAR_GRAPH:
		{
			return true; // lazy way out.
						 /*
						 float y = bitmapMetadata_->frameSize.height - drawAt;
						 // draw lit seqments
						 {
						 gmpi::drawing::Rect knob_rect(0.f, y, (float)bitmapMetadata_->frameSize.width, bitmapMetadata_->frameSize.height);
						 gmpi::drawing::Rect dest_rect(0.f, y, (float)bitmapMetadata_->frameSize.width, bitmapMetadata_->frameSize.height);
						 //dc->DrawBitmap(bitmap, &dest_rect, 1.0f, 1, &knob_rect);
						 }
						 // draw unlit seqments
						 {
						 gmpi::drawing::Rect knob_rect(0.f, bitmapMetadata_->frameSize.height, (float)bitmapMetadata_->frameSize.width, (float)bitmapMetadata_->frameSize.height + y);
						 gmpi::drawing::Rect dest_rect(0.f, 0.f, (float)bitmapMetadata_->frameSize.width, y);
						 //dc->DrawBitmap(bitmap, &dest_rect, 1.0f, 1, &knob_rect);
						 }
						 */
		}
		break;
		}

		int x = point.x + bmRect.left - bmOffset.width;
		int y = point.y + bmRect.top - bmOffset.height;

		if (x < bmRect.left || x > bmRect.right || y < bmRect.top || y > bmRect.bottom)
			return false;

		if (hitTestPixelAccurate)
		{
			auto pixels = bitmap_.lockPixels();
			auto pixel = pixels.getPixel(x, y);

			/* visualise hit test
			//		_RPT3(_CRT_WARN, "pixel(%d,%d) 0x%x\n", x, y, pixel);
			std::cout << "hittest( 0x" << std::hex << pixel << " )" << std::endl;

			{
			auto bms = bitmap_.GetSize();
			for(int yp = -20 ; yp < 20 ; ++yp)
			{
			for( int xp = -20 ; xp < 20 ;++xp)
			{
			int x2 = x + xp;
			int y2 = y + yp;

			if( x >= 0 && y >= 0 && x < bms.width && y < bms.height)
			{
			auto pixel2 = pixels.getPixel(x2,y2);
			int v = ((pixel2 >> 28) & 0x0f);
			std::cout << std::hex << v;
			}
			else
			{
			std::cout << '.';
			}
			}
			std::cout << std::endl;
			}
			}
			/*/

			return (((pixel >> 24) & 0xff) > 0);
		}
		else
		{
			return true;
		}
	}
public:
	enum class ToggleMode { Momentary, Alternate, OnOnly };
	ToggleMode toggleMode2;

	AnimatedBitmap() :
		drawAt(0)
		, m_rot_mode(0)
		, m_rotary_switch_start_angle(0.0)
		, m_rotary_switch_range(0.0)
		, last_frame_idx(0)
		, bitmapMetadata_(nullptr)
		, toggleMode2(ToggleMode::Momentary)
		, hitTestPixelAccurate(true)
	{
	}

	gmpi::drawing::Bitmap& getDrawBitmap()
	{
		return bitmap_;
	}

	void renderBitmap(gmpi::drawing::Graphics& dc, GmpiDrawing_API::MP1_SIZE_U topLeftU)
	{
		const gmpi::drawing::Size topLeft{ static_cast<float>(topLeftU.width), static_cast<float>(topLeftU.height) };

		auto lbitmap = getDrawBitmap();

		// Draw "not loaded" graphic.
		if (!gmpi::drawing::AccessPtr::get(lbitmap))
		{
			gmpi::drawing::Rect r{ topLeft.width, topLeft.height, topLeft.width + 8, topLeft.height + 10 };
			auto brush = dc.createSolidColorBrush(gmpi::drawing::Colors::White);
			dc.fillRectangle(r, brush);

			brush.setColor(gmpi::drawing::Colors::Black);
			auto tf = dc.getFactory().createTextFormat(6);
			dc.drawTextU("X", tf, r, brush);

			return;
		}

		float x = (float)bitmapMetadata_->padding_left;
		float y = (float)bitmapMetadata_->padding_top;
		//	_RPT2(_CRT_WARN, "BitmapWidget Padding x=%d,y=%d\n", x, y);

		gmpi::drawing::Rect dest_backgroundrect{ x, y, x + (float)bitmapMetadata_->frameSize.width, y + (float)bitmapMetadata_->frameSize.height };
		dest_backgroundrect = offsetRect(dest_backgroundrect, topLeft);

		switch (bitmapMetadata_->mode)
		{
		case ABM_ANIMATED:
		default:
		{
			gmpi::drawing::Rect source_rect{ 0.f, (float)drawAt, (float)bitmapMetadata_->frameSize.width, (float)(drawAt + bitmapMetadata_->frameSize.height) };

			//		_RPT4(_CRT_WARN, "DrawBitmap Dest(%f,%f,%f,%f) ", dest_rect.left, dest_rect.top, dest_rect.right, dest_rect.bottom);
			//		_RPT4(_CRT_WARN, "Source((%f,%f,%f,%f)\n", knob_rect.left, knob_rect.top, knob_rect.right, knob_rect.bottom);
			dc.drawBitmap(lbitmap, dest_backgroundrect, source_rect);
		}
		break;

		case ABM_SLIDER:
		{
			gmpi::drawing::Rect backgroundRect{ 0, 0, (float)bitmapMetadata_->frameSize.width, (float)bitmapMetadata_->frameSize.height };

			// draw background
			dc.drawBitmap(lbitmap, dest_backgroundrect, backgroundRect);

			// draw slider knob
			if (bitmapMetadata_->orientation == (int)ImageMetadata::MouseResponse::Vertical || bitmapMetadata_->orientation == (int)ImageMetadata::MouseResponse::ReverseVertical) // vertical
			{
				// fix for 'hamond' vertical draw bars
				// clip drawing to top of control
				float draw_pos = (float)drawAt;
				float source_y = bitmapMetadata_->handle_rect.top;
				float dest_height = getHeight(bitmapMetadata_->handle_rect);

				if (draw_pos < 0)
				{
					float chop_off_top = -draw_pos;
					draw_pos = 0;
					dest_height -= chop_off_top;
					source_y += chop_off_top;
				}

				gmpi::drawing::Rect knob_rect{ bitmapMetadata_->handle_rect.left, source_y, bitmapMetadata_->handle_rect.right, bitmapMetadata_->handle_rect.bottom };
				gmpi::drawing::Rect dest_rect{ bitmapMetadata_->handle_rect.left, draw_pos, bitmapMetadata_->handle_rect.right, draw_pos + dest_height };
				dest_rect = offsetRect(dest_rect, topLeft);
				dest_rect = offsetRect(dest_rect, { x, y });
				dc.drawBitmap(lbitmap, dest_rect, knob_rect);
			}
			else
			{
				// TODO, proper clipping on all sides !!!
				// fix for horizontal draw bars
				// clip drawing to left of control
				float draw_pos = (float)drawAt;
				float source_x = bitmapMetadata_->handle_rect.left;
				float dest_width = getWidth(bitmapMetadata_->handle_rect);

				if (draw_pos < 0)
				{
					float chop_off_left = -draw_pos;
					draw_pos = 0;
					dest_width -= chop_off_left;
					source_x += chop_off_left;
				}

				gmpi::drawing::Rect knob_rect{ source_x, bitmapMetadata_->handle_rect.top, bitmapMetadata_->handle_rect.right, bitmapMetadata_->handle_rect.bottom };
				gmpi::drawing::Rect dest_rect{ draw_pos, bitmapMetadata_->handle_rect.top, draw_pos + dest_width, bitmapMetadata_->handle_rect.bottom };
				dest_rect = offsetRect(dest_rect, topLeft);
				dest_rect = offsetRect(dest_rect, { x, y });
				dc.drawBitmap(lbitmap, dest_rect, knob_rect);
			}
		}
		break;

		case ABM_BAR_GRAPH:
		{
			float y = static_cast<float>(bitmapMetadata_->frameSize.height - drawAt);
			// draw lit seqments
			{
				gmpi::drawing::Rect knob_rect{ 0.f, y, (float)bitmapMetadata_->frameSize.width, bitmapMetadata_->frameSize.height };
				gmpi::drawing::Rect dest_rect{ 0.f, y, (float)bitmapMetadata_->frameSize.width, bitmapMetadata_->frameSize.height };
				dest_rect = offsetRect(dest_rect, topLeft);
				dc.drawBitmap(lbitmap, dest_rect, knob_rect);
			}
			// draw unlit seqments
			{
				gmpi::drawing::Rect knob_rect{ 0.f, bitmapMetadata_->frameSize.height, (float)bitmapMetadata_->frameSize.width, (float)bitmapMetadata_->frameSize.height + y };
				gmpi::drawing::Rect dest_rect{ 0.f, 0.f, (float)bitmapMetadata_->frameSize.width, y };
				dest_rect = offsetRect(dest_rect, topLeft);
				dc.drawBitmap(lbitmap, dest_rect, knob_rect);
			}
		}
		break;
		}
	}

	bool calcFrame(int frameNumber)
	{
		if (!gmpi::drawing::AccessPtr::get(bitmap_))
			return false;

		const auto oldDrawAt = drawAt;

		drawAt = frameNumber * bitmapMetadata_->frameSize.height;
		drawAt = std::clamp(drawAt, 0, (int) bitmap_.getSize().height - (int) bitmapMetadata_->frameSize.height);

		return oldDrawAt != drawAt;
	}

	bool calcDrawAt(float animationPosition)
	{
		if (!gmpi::drawing::AccessPtr::get(bitmap_))
			return false;

		animationPosition = (std::min)((std::max)(animationPosition, 0.0f), 1.0f);

		int draw_at = 0;

		switch (bitmapMetadata_->mode)
		{
		case ABM_ANIMATED:
		default:
		{
			// steped rotary mode?
			if (m_rotary_switch_range != 0.0)
			{
				// snap to nearest 'click'
//				float quantised_pos = animationPosition;

				if (m_rotary_steps < 2)
				{
//					quantised_pos = 0.0f;
					rotary_current_step = 0;
				}
				else
				{
//					quantised_pos = floorf(quantised_pos * (float)(m_rotary_steps - 0.00001f));
					rotary_current_step = static_cast<int>( floorf(animationPosition * (float)(m_rotary_steps - 0.00001f)) );
				}

				float quantised_pos = static_cast<float>(rotary_current_step) / (float)(m_rotary_steps - 1);

				float angle = m_rotary_switch_start_angle + m_rotary_switch_range * quantised_pos;

				//				quantised_pos = min(quantised_pos, m_rotary_switch_range);
				//				float angle = m_rotary_switch_start_angle + quantised_pos;
				if (angle < 0.0)
					angle += 1.0;

				if (angle > 1.0)
					angle -= 1.0;

				// convert radians to 0-1 range
                angle = (std::max)(angle, 0.f);
                angle = (std::min)(angle, 1.f);
				float frame_number = 0.5f + angle * (last_frame_idx + 1.f);
				draw_at = static_cast<int>(frame_number);

				if (draw_at > last_frame_idx) // last frame should be first repeated
					draw_at = 0;

				//				_RPT4(_CRT_WARN, "pos %f, st %f, range %f. drawat %d\n", pos,m_rotary_switch_start_angle, m_rotary_switch_range, nw_draw_at);
				//				_RPT1(_CRT_WARN, "angle %f\n", angle * 360.0);
				draw_at *= bitmapMetadata_->frameSize.height; // frame_size_y;
			}
			else // normal knob
			{
				// scale pos by number of frames (rounding to nearest)
				draw_at = static_cast<int>(0.5f + animationPosition * last_frame_idx);
				draw_at *= bitmapMetadata_->frameSize.height;
			}
		}
		break;

		case ABM_SLIDER:
		{
			draw_at = static_cast<int>(animationPosition * (bitmapMetadata_->handle_range_hi - bitmapMetadata_->handle_range_lo));
			// draw slider knob
			if (bitmapMetadata_->orientation == (int)ImageMetadata::MouseResponse::Vertical || bitmapMetadata_->orientation == (int)ImageMetadata::MouseResponse::ReverseVertical) // bitmap->GetMouseMode() == 0) // vertical
			{
				draw_at = bitmapMetadata_->handle_range_hi - draw_at;
			}
			else
			{
				draw_at = bitmapMetadata_->handle_range_lo + draw_at;
			}
		}
		break;

		case ABM_BAR_GRAPH:
		{
			draw_at = static_cast<int>(animationPosition * (bitmapMetadata_->line_end_length + 1));
			draw_at *= bitmapMetadata_->handle_range_hi;
			draw_at += bitmapMetadata_->handle_range_lo - 1;
		}
		break;
		}

		bool r = drawAt != draw_at;
		drawAt = draw_at;

		return r;
	}

	// Hit test can test if you clicked a lit pixel, or just test if you clicked in the rectangle.
	void setHitTestPixelAccurate(bool v)
	{
		hitTestPixelAccurate = v;
	}

	int32_t load(drawing::api::IFactory* guiHost, const char* imageFile, const char* textFile)
	{
		// Reset defaults.
		bitmapMetadata_ = nullptr;
		last_frame_idx = 0;
		bitmap_ = {};

		if (imageFile && imageFile[0] != 0) // not empty?
		{
			bitmap_ = GetImage(guiHost, imageFile, textFile, &bitmapMetadata_);

			if (gmpi::drawing::AccessPtr::get(bitmap_))
			{
				auto imageSize = bitmap_.getSize();
				last_frame_idx = (int)(((int)imageSize.height) / bitmapMetadata_->frameSize.height - 1);
				return gmpi::MP_OK;
			}
		}
		return gmpi::MP_FAIL;
	}

	void setRotarySwitch(int stepCount, float p_start_angle, float p_range)
	{
		m_rotary_switch_start_angle = p_start_angle;
		m_rotary_switch_range = p_range;
		m_rotary_steps = stepCount;
	}

	int getRotaryCurrentStep()
	{
		return rotary_current_step;
	}

	const ImageMetadata* metadata() const
	{
		return bitmapMetadata_;
	}
};

} // namespace gmpi_helper
