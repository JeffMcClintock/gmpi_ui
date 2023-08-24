#include <string>
#include <vector>
#include <algorithm>
#include "ImageMetadata.h"

using namespace std;
using namespace gmpi_sdk;

int StringToInt(const string& string)
{
	const int p_base = 10;
	assert(string.size() < 11 || string.find(L',') != wstring::npos); // else too many digits overflows sizeof int
	char* temp;
	return static_cast<int>(strtol(string.c_str(), &temp, p_base));
}

void SplitString(const char* pText, std::vector<std::string>& returnValue)
{
	if( pText )
	{
		const char SINGLE_QUOTE = '\'';
		const char DOUBLE_QUOTE = '"';
		const unsigned char* begin = ( const unsigned char*) pText;

		char terminator;

		while( true )
		{
			// eat leading spaces.
			while( isspace(*begin) )
			{
				++begin;
			}

			if( *begin == 0 ) // end of string?
				break;

			const unsigned char* end = begin + 1;

			if( *begin == SINGLE_QUOTE || *begin == DOUBLE_QUOTE )
			{
				terminator = *begin;
				++begin;
				end = begin;
				while( *end != terminator && *end != 0 )
				{
					++end;
				}
			}
			else
			{
				terminator = ' '; // String might be terminated by space or by quote starting next item.
				while( !isspace(*end) && *end != SINGLE_QUOTE && *end != DOUBLE_QUOTE && *end != 0 )
				{
					if( *end == ',' ) // word might end in comma.
					{
						terminator = *end;
						break;
					}
					++end;
				}
			}

			// We have now isolated a 'word'.
			returnValue.push_back(std::string(begin, end));

			// If word ends on a space then comma, e.g. " cat , dog" skip the comma.
			if( *end == ' ' )
			{
				while( *end != 0 )
				{
					char c = *end++;

					if( (*end != ' ' && *end != ',') || c == ',' )
					{
						break;
					}
				}
			}

			// Skip closing quote/comma if nesc.
			if( *end == terminator && terminator != ' ' )
			{
				++end;
			}

			begin = end;
		}
	}
}

void ImageMetadata::Serialise(mp_shared_ptr<gmpi::IProtectedFile2> stream)
{
	Reset();

	int64_t fileLength = 0;
	stream->getSize(&fileLength);
	std::string imageMetadata;
	imageMetadata.assign((size_t)fileLength, 0);
	stream->read(&(imageMetadata[0]), fileLength);

	// Parse image metadata.

	// Split to lines.
	std::vector<std::string> lines;
	std::string::size_type pos = 0;
	std::string::size_type prev = 0;
	while ((pos = imageMetadata.find('\n', prev)) != std::string::npos)
	{
		auto count = pos - prev;
		if (imageMetadata[prev + count - 1] == '\r')
		{
			--count;
		}

		lines.push_back(imageMetadata.substr(prev, count));
		prev = pos + 1;
	}

	// To get the last (or only) line (if delimiter is not found).
	lines.push_back(imageMetadata.substr(prev));

	for (auto& line : lines)
	{
		// comments are lines begining with a semicolon ; like this
		if (!line.empty() && line[0] == ';')
			continue;
		
		// Split to words.
		std::vector<std::string> words;
		words.clear();
		SplitString(line.c_str(), words);

		{
//			bool found_image_size = false;

			if (words.size() > 0)
			{
				//				_RPT1(_CRT_WARN, "%s\n", words[0] );
				if (words[0] == "type")
				{
					if (words.size() > 1)
					{
						if (words[1] == "slider")
						{
							mode = ABM_SLIDER;
						}

						if (words[1] == "knob" || words[1] == "animated")
						{
							mode = ABM_ANIMATED;
						}

						if (words[1] == "button")
						{
							mode = ABM_BUTTON;
						}

						if (words[1] == "bargraph")
						{
							mode = ABM_BAR_GRAPH;
						}
					}
				}

				if (words[0] == "frame_size")
				{
					if (words.size() > 2)
					{
						frameSize.width = StringToInt(words[1]);
						frameSize.height = StringToInt(words[2]);
//						found_image_size = true;
					}
				}

				if (words[0] == "padding")
				{
					if (words.size() > 4)
					{
						padding_top = (float)StringToInt(words[1]);
						padding_bottom = (float)StringToInt(words[2]);
						padding_left = (float)StringToInt(words[3]);
						padding_right = (float)StringToInt(words[4]);
					}
				}

				if (words[0] == "handle_rect")
				{
					if (words.size() > 4)
					{
						handle_rect.left = (float)StringToInt(words[1]);
						handle_rect.top = (float)StringToInt(words[2]);
						handle_rect.right = (float)StringToInt(words[3]);
						handle_rect.bottom = (float)StringToInt(words[4]);
					}
				}

				if (words[0] == "handle_range" || words[0] == "segment")
				{
					if (words.size() > 1)
					{
						handle_range_lo = StringToInt(words[1]);
						handle_range_hi = StringToInt(words[2]);
					}
				}

				if (words[0] == "orientation" || words[0] == "tiled" || words[0] == "mouse_response")
				{
					if (words.size() > 0)
					{
						if (words[1][0] == 'f') // background not tiled (false)
						{
							orientation = (int)BitmapTiling::NotTiled; //1;
						}

						if (words[1][0] == 't') // background tiled (true)
						{
							orientation = (int)BitmapTiling::Tiled; //  0;
						}

						if (words[1][0] == 'h') // horizontal
						{
							orientation = (int)MouseResponse::Horizontal; // 1;
						}

						if (words[1][0] == 'v') // vertical
						{
							orientation = (int)MouseResponse::Vertical; // 0;
						}

						if (words[1][0] == 'c') // click on/off (button)
						{
							orientation = (int)MouseResponse::ClickToggle; // 2;
						}

						if (words[1][0] == 's') // stepped clicks
						{
							orientation = (int)MouseResponse::Stepped; // 4;
						}

						if (words[1][0] == 'r') // rotary / vertical
						{
							if (words[1][1] == 'e')
							{
								orientation = (int)MouseResponse::ReverseVertical; // 3;
							}
							else
							{
								orientation = (int)MouseResponse::Rotary; // 3;
							}
						}
					}
				}

				if (words[0] == "line_end_length" || words[0] == "segment_count")
				{
					if (words.size() > 0)
					{
						line_end_length = (float)StringToInt(words[1]);
					}
				}

				if (words[0] == "transparent_pixel")
				{
					if (words.size() > 1)
					{
						transparent_pixelX = StringToInt(words[1]);
						transparent_pixelY = StringToInt(words[2]);
					}
				}
			}
		}
	}
}


void SkinMetadata::Serialise(mp_shared_ptr<gmpi::IProtectedFile2> stream)
{
	fonts_.clear();

	int64_t fileLength = 0;
	stream->getSize(&fileLength);
	std::string imageMetadata;
	imageMetadata.assign((size_t)fileLength, 0);
	stream->read(&( imageMetadata[0] ), fileLength);

	// Parse image metadata.

	// Split to lines.
	std::vector<std::string> lines;
	std::string::size_type pos = 0;
	std::string::size_type prev = 0;
	while( ( pos = imageMetadata.find('\n', prev) ) != std::string::npos )
	{
		auto count = pos - prev;
		if( (prev + count) > 0 && imageMetadata[prev + count - 1] == '\r' )
		{
			--count;
		}

		lines.push_back(imageMetadata.substr(prev, count));
		prev = pos + 1;
	}

	// To get the last substring (or only, if delimiter is not found)
	lines.push_back(imageMetadata.substr(prev));

	FontMetadata* current = 0;
	for (auto& line : lines)
	{
		// Split to words.
		std::vector<std::string> words;
		words.clear();
		SplitString(line.c_str(), words);

		{
			if( words.size() > 0 )
			{
				//				_RPT1(_CRT_WARN, "%s\n", words[0] );
				if( words[0] == "FONT_CATEGORY" )
				{
					if( words.size() > 1 )
					{
						string category = words[1];
						transform(category.begin(), category.end(), category.begin(), towlower);

						fonts_.push_back(std::make_unique<FontMetadata>(category));

						current = fonts_.back().get();
					}
				}

				if( !fonts_.empty() && words.size() > 1 && current )
				{
					if( words[0] == "font-family" )
					{
						for( size_t i = 1; i < words.size();  ++i )
						{
							current->faceFamilies_.push_back(words[i]);
						}
					}

					if (words[0] == "font-size")
					{
						current->pixelHeight_ = current->size_ = StringToInt(words[1]);
					}

					if (words[0] == "vst3-vertical-offset")
					{
						current->vst3_vertical_offset_ = StringToInt(words[1]);
					}
					if (words[0] == "legacy-vertical-offset")
					{
						current->verticalSnapBackwardCompatibilityMode = (0 != StringToInt(words[1]));
					}
					if (words[0] == "GDI_pixelHeight")
					{
						current->pixelHeight_ = StringToInt(words[1]);
					}
					if (words[0] == "GDI_pixelWidth")
					{
						current->pixelWidth_ = StringToInt(words[1]);
					}

					if( words[0] == "font-color" )
					{
						uint32_t c;
						if (words[1][0] == '#')
						{
							const int p_base = 16;
							char* temp;
							c = static_cast<uint32_t>(strtol(words[1].c_str() + 1, &temp, p_base));
						}
						else
						{
							c = StringToInt(words[1]);
						}
						// convert g-b-r to more standard a-r-g-b . SE text files do not support alpha, so default alpha to 1.0
						current->color_ = (c >> 16) | (c << 16) | (c & 0xff00) | 0xff000000;
					}

					if( words[0] == "background-color" )
					{
						//if (current->backgroundColor_ == -1) // means "none". Fix.
						//{
						//	current->backgroundColor_ = 0; // transparent black.
						//}
						uint32_t c;
						if (words[1][0] == '#')
						{
							const int p_base = 16;
							char* temp;
							c = static_cast<uint32_t>(strtol(words[1].c_str() + 1, &temp, p_base));
						}
						else
						{
							c = StringToInt(words[1]);
						}
						// convert g-b-r to more standard a-r-g-b . SE text files do not support alpha, so default alpha to 1.0
						current->backgroundColor_ = (c >> 16) | (c << 16) | (c & 0xff00) | 0xff000000;
					}

					if( words[0] == "font-weight" )
					{
						if( tolower(words[1][0]) == 'b' ) // Bold
						{
							current->flags_ |= TTL_BOLD;
						}
						else
						{
							if( tolower(words[1][0]) == 'l' ) // Light
							{
								current->flags_ |= TTL_LIGHT;
							}
							else // numeric spec
							{
								int fw = StringToInt(words[1]);

								if( fw != 0 )
								{
									if( fw < 301 )
										current->flags_ |= TTL_LIGHT;
									else if( fw > 550 )
										current->flags_ |= TTL_BOLD;
								}
							}
						}
					}

					if( words[0] == "text-align" )
					{
						if( tolower(words[1][0]) == 'r' ) // Right
						{
							current->flags_ |= TTL_RIGHT;
						}
						else
						{
							if( tolower(words[1][0]) == 'c' ) // Right
							{
								current->flags_ |= TTL_CENTERED;
							}
						}
					}

					if( words[0] == "text-decoration" )
					{
						if( tolower(words[1][0]) == 'u' ) // Underline
						{
							current->flags_ |= TTL_UNDERLINE;
						}
					}
					
					if( words[0] == "font-style" )
					{
						if( tolower(words[1][0]) == 'i' ) // Italic
						{
							current->flags_ |= TTL_ITALIC;
						}
					}
				}
			}
		}
	}

	// compensate for incomplete font information.
	for (auto&f : fonts_)
	{
		if (f->faceFamilies_.empty())
		{
			f->faceFamilies_.push_back("Verdana");
		}
	}
}

const FontMetadata* SkinMetadata::getFont(std::string category) const
{
	transform(category.begin(), category.end(), category.begin(), towlower);

	for (auto& f : fonts_)
	{
		if( f->category_ == category )
		{
			return f.get();
		}
	}
	return defaultFont_.get();
}

SkinMetadata::SkinMetadata()
{
	defaultFont_ = std::make_unique<FontMetadata>("default");
	defaultFont_->faceFamilies_.push_back("Arial");
	defaultFont_->color_ = 0xff000000 | GmpiDrawing::Color::Black; // From SE.
}
