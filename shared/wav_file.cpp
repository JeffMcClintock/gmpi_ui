#define _SILENCE_ALL_CXX17_DEPRECATION_WARNINGS 1
#include "wav_file.h"

#include <algorithm>
#include <climits>
#include <vector>
#include <sstream>
#include <assert.h>
#include <codecvt>
#include <locale>

#ifndef WAVE_FORMAT_PCM
#define WAVE_FORMAT_PCM     1
#endif

using namespace std;

struct wave_file_header
{
	char chnk1_name[4];
	int32_t chnk1_size;
	char chnk2_name[4];
	char chnk3_name[4];
	int32_t chnk3_size;
	uint16_t wFormatTag;
	uint16_t nChannels;
	int32_t nSamplesPerSec;
	int32_t nAvgBytesPerSec;
	uint16_t nBlockAlign;
	uint16_t wBitsPerSample;
	char chnk4_name[4];
	int32_t chnk4_size;
};

#ifdef _WIN32
std::wstring toOperatingSystemFilename(const std::string& filename)
{
	// Windows will fail to open UTF8 filenames, but will handle UTF16 OK.
	static std::wstring_convert<std::codecvt_utf8<wchar_t> > convert;
	return convert.from_bytes(filename);
}
#else
std::string toOperatingSystemFilename(const std::string& filename)
{
	return filename;
}
#endif


// -- construction --

// ---------------------------------------------------------------------------
//	WavFile constructor from filename
// ---------------------------------------------------------------------------
//!	Initialize an instance of WavFile by importing sample data from
//!	the file having the specified filename or path.
//!
//!	\param filename is the name or path of an AIFF samples file
//
WavFile::WavFile( const std::string & filename, int maxChannels, int extraInterpolationSamples) :
	rate_( 1 ),		// rate will be overwritten on import
    numchans_( 1 ),
	numFrames_(0)
{
	readWavData( filename, maxChannels, extraInterpolationSamples);
}


// -- export --

// ---------------------------------------------------------------------------
//	write 
// ---------------------------------------------------------------------------
//!	Export the sample data represented by this WavFile to
//!	the file having the specified filename or path. Export
//!	signed integer samples of the specified size, in bits
//!	(8, 16, 24, or 32).
//!
//!	\param filename is the name or path of the AIFF samples file
//!	to be created or overwritten.
//!	\param bps is the number of bits per sample to store in the
//!	samples file (8, 16, 24, or 32).If unspeicified, 16 bits
//!	is assumed.
//
void WavFile::write( const std::string & filename, unsigned int bps )
{
	static const unsigned int ValidSizes[] = { 8, 16, 24, 32 };
	if ( std::find(std::begin(ValidSizes), std::end(ValidSizes), bps ) == std::end(ValidSizes))
	{
		throw( "Invalid bits-per-sample." );
	}

	{
		bool floatFormat = bps == 32; // false for 16-bit PCM
		int bits_per_sample = bps;
		uint16_t n_channels = 1;
		int sample_count = 0;
		int sample_rate = (int) rate_;
		float* src = 0;

		sample_count = (int) samples_.size();
		src = samples_.data();

		wave_file_header wav_head;
		memcpy(wav_head.chnk1_name, "RIFF", 4);
		memcpy(wav_head.chnk2_name, "WAVE", 4);
		memcpy(wav_head.chnk3_name, "fmt ", 4);
		memcpy(wav_head.chnk4_name, "data", 4);

		if (floatFormat)
		{
			wav_head.wFormatTag = WAVE_FORMAT_IEEE_FLOAT;
			bits_per_sample = 32;
		}
		else
		{
			wav_head.wFormatTag = WAVE_FORMAT_PCM;
			bits_per_sample = bps;
		}

		wav_head.wBitsPerSample = static_cast<uint16_t>(bits_per_sample);
		wav_head.chnk3_size = 16;
		wav_head.nChannels = n_channels;
		wav_head.chnk4_size = (int32_t)(sample_count * wav_head.wBitsPerSample / 8 * wav_head.nChannels);
		wav_head.chnk1_size = wav_head.chnk4_size + 36;
		wav_head.nSamplesPerSec = sample_rate;
		wav_head.nAvgBytesPerSec = wav_head.nSamplesPerSec * wav_head.nChannels * wav_head.wBitsPerSample / 8;
		wav_head.nBlockAlign = (wav_head.wBitsPerSample / 8) * wav_head.nChannels;

		ofstream myfile;
		myfile.open(toOperatingSystemFilename(filename), ios_base::out | ios_base::binary | ios_base::trunc);
		if (!myfile)
		{
			throw("Can't save wave file.");
		}

		myfile.write((char*)&wav_head, 44);
		float* samples[2];
		for (int c = 0; c < wav_head.nChannels; ++c)
		{
			samples[c] = src;
		}

		if (floatFormat)
		{
			for (int i = 0; i < sample_count; ++i)
			{
				for (int c = 0; c < wav_head.nChannels; ++c)
				{
					float s = (float) *samples[c];

					myfile.write((char*)&s, sizeof(float));
					samples[c]++;
				}
			}
		}
		else
		{
			int bytesPerSample = bps / 8;
			double multiplier = (double)( 1 << (bps - 1));
			int sample_int;
			char* data = (char*)&sample_int;
			int add = bps == 8 ? 128 : 0;
			for (int i = 0; i < sample_count; ++i)
			{
				for (int c = 0; c < wav_head.nChannels; ++c)
				{
					double s = *samples[c];
					sample_int = add + (int)(0.5 + s * multiplier);

					myfile.write(data, bytesPerSample);
					samples[c]++;
				}
			}
		}

		myfile.close();
	}

}

// -- access --

// ---------------------------------------------------------------------------
//	numChannels 
// ---------------------------------------------------------------------------
//!	Return the number of channels of audio samples represented by
//! this WavFile, 1 for mono, 2 for stereo.
//
unsigned int 
WavFile::numChannels( void ) const
{
    return numchans_;
}

// ---------------------------------------------------------------------------
//	numFrames 
// ---------------------------------------------------------------------------
//!	Return the number of sample frames represented in this WavFile.
//!	A sample frame contains one sample per channel for a single sample
//!	interval (e.g. mono and stereo samples files having a sample rate of
//!	44100 Hz both have 44100 sample frames per second of audio samples).
//
 WavFile::size_type  
 WavFile::numFrames( void ) const
 {
 	return numFrames_;
 }

// ---------------------------------------------------------------------------
//	sampleRate 
// ---------------------------------------------------------------------------
//!	Return the sampling freqency in Hz for the sample data in this
//!	WavFile.
//
double  
WavFile::sampleRate( void ) const
{
	return rate_;
}

// ---------------------------------------------------------------------------
//	samples 
// ---------------------------------------------------------------------------
//!	Return a reference (or const reference) to the vector containing
//!	the floating-point sample data for this WavFile.
//
WavFile::samples_type & 
WavFile::samples( void )
{
	return samples_;
}

//!	Return a const reference (or const reference) to the vector containing
//!	the floating-point sample data for this WavFile.
//
const WavFile::samples_type & 
WavFile::samples( void ) const
{
	return samples_;
}


// ---------------------------------------------------------------------------
//	readAiffData
// ---------------------------------------------------------------------------
//	Import data from an AIFF file on disk.
//
void WavFile::readWavData( const std::string & filename, int maxChannels, int extraInterpolationSamples )
{
	ifstream myfile;
	myfile.open(toOperatingSystemFilename(filename), ios_base::in | ios_base::binary);
	if (!myfile)
	{
// not a bug.		throw("File not found, or corrupted.");
		return;
	}

	MYWAVEFORMATEX waveheader;
	char* wave_data = 0;
	unsigned int wave_data_bytes{};

	memset(&waveheader, 0, sizeof(waveheader));

	int chunkLength;
	char chunkName[4];
	myfile.read((char*)&chunkName, 4);

	if (chunkName[0] != 'R' || chunkName[1] != 'I' || chunkName[2] != 'F' || chunkName[3] != 'F') // RIFF.
	{
		throw("Input stream doesn't comply with the RIFF specification");
		return;
	}

	myfile.read((char*)&chunkLength, 4);

	// WAVE chunk.
	myfile.read((char*)&chunkName, 4);
	if (chunkName[0] != 'W' || chunkName[1] != 'A' || chunkName[2] != 'V' || chunkName[3] != 'E')
	{
		throw( "Input stream doesn't comply with the WAVE specification");
		return;
	}

	while (!myfile.eof())
	{
		chunkName[0] = 0;
		chunkLength = 0;
		myfile.read((char*)&chunkName, 4);
		myfile.read((char*)&chunkLength, 4);

		if (chunkName[0] == 'f' && chunkName[1] == 'm' && chunkName[2] == 't' && chunkName[3] == ' ') //  "fmt "
		{
			myfile.read((char*)&waveheader, (std::min)((size_t)chunkLength, sizeof(waveheader)));
			if (waveheader.wBitsPerSample == 0) // this.SignificantBitsPerSample == 0)
			{
				throw( "The input stream uses an unhandled SignificantBitsPerSample parameter");
				return;
			}
			if (chunkLength > sizeof(waveheader))
			{
				myfile.ignore(chunkLength - sizeof(waveheader));
			}
		}
		else
		{
			if (chunkName[0] == 'd' && chunkName[1] == 'a' && chunkName[2] == 't' && chunkName[3] == 'a') //  "data"
			{
				wave_data_bytes = chunkLength;
				assert(wave_data == nullptr);
				wave_data = new char[wave_data_bytes];
				myfile.read(wave_data, wave_data_bytes);
			}
			else
			{
				// Next chunk.
				myfile.ignore(chunkLength);
			}
		}
	}

	rate_ = waveheader.nSamplesPerSec;
	numFrames_ = wave_data_bytes / waveheader.nBlockAlign;
	numchans_ = std::min(maxChannels, (int)waveheader.nChannels);
//	int channelsIgnored = waveheader.nChannels - numchans_;

	int totalSamples = numchans_ * (numFrames_ + extraInterpolationSamples * 2);

	samples_.assign(totalSamples, 0.0);

	float* dest = samples_.data();
	{
		for (unsigned int channel = 0; channel < numchans_; ++channel)
		{
			for (int i = 0; i < extraInterpolationSamples; ++i)
			{
				*dest++ = 0.0f;
			}

			switch (waveheader.wFormatTag)
			{
			case WAVE_FORMAT_PCM:
			{
				switch (waveheader.wBitsPerSample)
				{
				case 8:
				{
					//for (int i = 0; i < numFrames_; ++i)
					//{
					//	*dest++ = multiplier * (((unsigned char*)wave_data)[j] - 128.0f);
					//	j += waveheader.nChannels;
					//}

					constexpr float toFloatMultiplier = 1.f / (1 << 7);

					auto i8 = ((unsigned char*)wave_data) + channel;
					for (int i = 0; i < numFrames_; ++i)
					{
						int32_t s = *i8 - 0x80;
						*dest++ = toFloatMultiplier * s;
						i8 += waveheader.nChannels;
					}
				}
				break;

				case 16:
				{
					//for (int i = 0; i < numFrames_; ++i)
					//{
					//	*dest++ = multiplier * ((short*)wave_data)[j];
					//	j += waveheader.nChannels;
					//}

					constexpr float toFloatMultiplier = 1.f / (1 << 15);

					auto i16 = ((int16_t*)wave_data) + channel;
					for (int i = 0; i < numFrames_; ++i)
					{
						*dest++ = toFloatMultiplier * *i16;
						i16 += waveheader.nChannels;
					}
				}
				break;

				case 24:
				{
					constexpr int sampleBytes = 3;
					//unsigned char* src = (unsigned char*)wave_data + sampleBytes * channel;

					//for (int i = 0; i < numFrames_; ++i)
					//{
					//	int intSample = (src[0] << 8) + (src[1] << 16) + (src[2] << 24);
					//	*dest++ = multiplier * intSample;
					//	src += sampleBytes * waveheader.nChannels;
					//}

					constexpr float toFloatMultiplier = 1.f / (1 << 31);

					auto i24 = ((unsigned char*)wave_data) + sampleBytes * channel;
					for (int i = 0; i < numFrames_; ++i)
					{
						const int32_t t = (i24[0] << 8) + (i24[1] << 16) + (i24[2] << 24);
						*dest++ = toFloatMultiplier * t;
						i24 += sampleBytes * waveheader.nChannels;
					}
				}
				break;

				case 32:
				{
					//for (int i = 0; i < numFrames_; ++i)
					//{
					//	*dest++ = multiplier * ((int32_t*)wave_data)[j];
					//	j += waveheader.nChannels;
					//}

					constexpr float toFloatMultiplier = 1.f / (1 << 31);

					auto i32 = ((int32_t*)wave_data) + channel;
					for (int i = 0; i < numFrames_; ++i)
					{
						*dest++ = toFloatMultiplier * *i32;
						i32 += waveheader.nChannels;
					}
				}
				break;

				default:
				{
					//					message(L"This WAVE file format is not supported.  Convert it to 16 bit uncompressed mono or stereo.");
				}

				}
			}

			break;

			case 03: // WAVE_FORMAT_IEEE_FLOAT:
				if (waveheader.wBitsPerSample == 32)
				{
					//for (int i = 0; i < numFrames_; ++i)
					//{
					//	*dest++ = ((float*)wave_data)[j];
					//	j += waveheader.nChannels;
					//}

					auto f32 = ((float*)wave_data) + channel;
					for (int i = 0; i < numFrames_; ++i)
					{
						*dest++ = *f32;
						f32 += waveheader.nChannels;
					}
				}
				break;

			default:
				//		message(L"This WAVE file format is not supported.  Convert it to PCM or Float.");
				;
			};
		}
	}

	delete[] wave_data;
}

std::unique_ptr<WavFileCursor> WavFileStreaming::open(const std::string& pfilename)
{
	filename = pfilename;

	wavedata_offset = std::streampos(-1);
	totalSampleFrames = 0;
	memset(&waveheader, 0, sizeof(waveheader));
	memset(&m_sampler_data, 0, sizeof(m_sampler_data));

	ifstream myfile;
	myfile.open(toOperatingSystemFilename(filename), ios_base::in | ios_base::binary);
	if (!myfile)
	{
		// not a bug.		throw("File not found, or corrupted.");
		return nullptr;
	}

	unsigned int chunkLength;
	char chunkName[4];
	myfile.read((char*)&chunkName, 4);

	if (chunkName[0] != 'R' || chunkName[1] != 'I' || chunkName[2] != 'F' || chunkName[3] != 'F') // RIFF.
	{
		throw("Input stream doesn't comply with the RIFF specification");
//		return nullptr;
	}

	myfile.read((char*)&chunkLength, 4);

	// WAVE chunk.
	myfile.read((char*)&chunkName, 4);
	if (chunkName[0] != 'W' || chunkName[1] != 'A' || chunkName[2] != 'V' || chunkName[3] != 'E')
	{
		throw("Input stream doesn't comply with the WAVE specification");
//		return nullptr;
	}

	while (!myfile.eof())
	{
		chunkName[0] = 0;
		chunkLength = 0;
		myfile.read((char*)&chunkName, 4);
		myfile.read((char*)&chunkLength, 4);

		if (chunkName[0] == 'f' && chunkName[1] == 'm' && chunkName[2] == 't' && chunkName[3] == ' ') //  "fmt "
		{
			const size_t expectedSize = sizeof(waveheader);
			myfile.read((char*)&waveheader, (std::min)((size_t)chunkLength, expectedSize));
			if (waveheader.wBitsPerSample == 0)
			{
				throw("The input stream uses an unhandled SignificantBitsPerSample parameter");
//				return nullptr;
			}

			// Float wave sample should be 32 bit, hovever cooledit 96 saves
			// waves as 16 bit INTEGERS (and uses headertag FLOAT, confused?)
			if (waveheader.wFormatTag == 0x0003 /*WAVE_FORMAT_IEEE_FLOAT*/ && waveheader.wBitsPerSample < 32)
			{
				waveheader.wFormatTag = WAVE_FORMAT_PCM;
			}

			if (chunkLength > expectedSize)
			{
				myfile.ignore(chunkLength - expectedSize);
			}
		}
		else
		{
			if (chunkName[0] == 'd' && chunkName[1] == 'a' && chunkName[2] == 't' && chunkName[3] == 'a') //  "data"
			{
				totalSampleFrames = chunkLength / waveheader.nBlockAlign;

				// note fileposition of first sample.
				wavedata_offset = myfile.tellg();

				/* read immediate.
				assert(wave_data == nullptr);
				wave_data = new char[wave_data_bytes];
				myfile.read(wave_data, wave_data_bytes);
				*/

				myfile.ignore(chunkLength);
			}
			else
			{
				if (chunkName[0] == 's' && chunkName[1] == 'm' && chunkName[2] == 'p' && chunkName[3] == 'l') //  "smpl"
				{
					const size_t expectedSize = sizeof(m_sampler_data);
					myfile.read((char*)&m_sampler_data, (std::min)((size_t)chunkLength, expectedSize));
					if (chunkLength > expectedSize)
					{
						myfile.ignore(chunkLength - expectedSize);
					}

					// cope with corrupt files
					if (m_sampler_data.Loops[0].dwStart < 0 || m_sampler_data.Loops[0].dwStart >= totalSampleFrames || m_sampler_data.Loops[0].dwEnd < 0 || m_sampler_data.Loops[0].dwEnd >= totalSampleFrames)
					{
						m_sampler_data.cSampleLoops = 0;
					}
				}
				else
				{
					// Next chunk.
					myfile.ignore(chunkLength);
				}
			}
		}
	}

	switch (waveheader.wFormatTag)
	{
	case WAVE_FORMAT_PCM:
		if (waveheader.wBitsPerSample != 8 && waveheader.wBitsPerSample != 16 && waveheader.wBitsPerSample != 24 && waveheader.wBitsPerSample != 32)
		{
			throw("This WAVE file format is not supported.  Convert it to 16 bit uncompressed mono or stereo.");
//			return nullptr;
		}

		break;

	case WAVE_FORMAT_IEEE_FLOAT:
		if (waveheader.wBitsPerSample != 32 ) //&& waveheader.wBitsPerSample != 64)
		{
			throw("This WAVE file format is not supported.  Convert it to 16 bit uncompressed mono or stereo.");
//			return nullptr;
		}
		break;

	default:
		throw("This WAVE file format is not supported.  Convert it to PCM or Float");
//		return nullptr;
	};

	if (totalSampleFrames == 0 || wavedata_offset == std::streampos(-1))
		return nullptr;

	return std::make_unique<WavFileCursor>(this);
}

void WavFileCursor::DiskSamplesToBuffer(MYWAVEFORMATEX& waveheader, int sampleReadCount, float* dest)
{
	{
		auto readBytes = sampleReadCount * waveheader.wBitsPerSample / 8;

		// TODO for float, just read direct into buffer, for int32, read indirect, then convert in-place.
		myfile.clear();
		myfile.read(conversionBuffer.data(), readBytes);
		assert(!myfile.fail());
	}

	const auto source = conversionBuffer.data();
	int c = sampleReadCount;

	// convert samples
	switch (waveheader.wFormatTag)
	{
		case WAVE_FORMAT_PCM:
		{
			switch (waveheader.wBitsPerSample)
			{
			case 8:
			{
				constexpr float toFloatMultiplier = 1.f / (1 << 7);

				auto i8 = (unsigned char*)source;
				while (c-- > 0)
				{
					int32_t s = *i8++ - 0x80;
					*dest++ = toFloatMultiplier * s;
				}
			}
			break;

			case 16:
			{
				constexpr float toFloatMultiplier = 1.f / (1 << 15);

				auto i16 = (int16_t*)source;
				while (c-- > 0)
				{
					*dest++ = toFloatMultiplier * *i16++;
				}
			}
			break;

			case 24:
			{
				constexpr float toFloatMultiplier = 1.f / (1 << 31);

				auto i24 = (unsigned char*)source;
				while (c-- > 0)
				{
					const int32_t t = (i24[0] << 8) + (i24[1] << 16) + (i24[2] << 24);
					*dest++ = toFloatMultiplier * t;
					i24 += 3;
				}
			}
			break;

			case 32:
			{
				constexpr float toFloatMultiplier = 1.f / (1 << 31);

				auto i32 = (int32_t*)source;
				while (c-- > 0)
				{
					*dest++ = toFloatMultiplier * *i32++;
				}
			}
			break;

			}
		}
		break;

		case WAVE_FORMAT_IEEE_FLOAT:
		{
			auto f32 = (float*)source;
			while (c-- > 0)
			{
				*dest++ = *f32++;
			}
		}
		break;
	}
}

// !! buggy with looping, results in a few zero samples at loop point. ref 'Sine Looped (hard)_8bit_mono.wav'
std::tuple<const float*, int> WavFileCursor::GetMoreSamples(bool gate)
{
	// copy overlap from last buffer.
	const int overlapSamples = SampleBufferOverlap * ChannelsCount();
	const int overlapSamplesTotal = overlapSamples * 2;

	// Copy end of last buffer to start of this one.
	for (int i = 0 ; i < overlapSamplesTotal; ++i )
		buffer[i] = buffer[lastSentSampleIndex + i];

#ifdef _DEBUG
	for (size_t i = overlapSamplesTotal; i < buffer.size(); ++i)
		buffer[i] = 1000000.f;
#endif

	int bufferWriteStart;
	int64_t filePosition;

	if (samplePosition == loopEndMarker && gate) // reached loop point?
	{
		// Rewind file to loop start.
		samplePosition = sampleData->m_sampler_data.Loops[0].dwStart * ChannelsCount();
		int64_t advanceBytes = sampleData->waveheader.nBlockAlign * (samplePosition / ChannelsCount());
		myfile.seekg(sampleData->wavedata_offset);
		myfile.seekg(advanceBytes, ios_base::cur);

		// Keep half of the buffered samples from before the loop end,
		// but write over right-hand side of loop point with samples from start of loop.
		bufferWriteStart = overlapSamples;
		filePosition = samplePosition;
	}
	else
	{
		bufferWriteStart = overlapSamplesTotal;
		filePosition = samplePosition + overlapSamples;
	}

	int64_t sampleReadCount;
	if (samplePosition < loopEndMarker)
	{
		sampleReadCount = loopEndMarker - samplePosition; // read 4-samples past loop-end. Might fail to loop if file not that long.
	}
	else
	{
		sampleReadCount = sampleData->totalSamples() - samplePosition; // not  - filePosition;
	}
	assert(sampleReadCount >= 0);

	// Limit to available buffer size.
	int64_t spaceInBuffer = static_cast<int>(buffer.size()) - bufferWriteStart;
	sampleReadCount = (std::min)(sampleReadCount, spaceInBuffer);
	int64_t returnSamplesCount = sampleReadCount - (overlapSamplesTotal - bufferWriteStart);

	// have we gone "off end" of sample? pad with zeros.
    const int64_t remainingDiskSamples = sampleData->totalSamples() - filePosition;
    const int64_t zeroPadding = (std::max)((int64_t)0, sampleReadCount - remainingDiskSamples);
 
    sampleReadCount -= zeroPadding;  // 0 -> 4 frames.

    for (int i = 0; i < zeroPadding; ++i)
        buffer[bufferWriteStart + sampleReadCount + i] = 0.f;
    
    if(sampleReadCount > 0)
    {
        // Read samples off disk.
        auto dest = buffer.data() + bufferWriteStart;
        // _RPT1(_CRT_WARN, "DiskSamplesToBuffer %d\n", static_cast<int>(sampleReadCount));
        DiskSamplesToBuffer(sampleData->waveheader, static_cast<int>(sampleReadCount), dest);
    }
	auto returnData = buffer.data() + overlapSamples;
	int returnSamples = static_cast<int>(returnSamplesCount);
	lastSentSampleIndex = returnSamplesCount;

	// For very first buffer, the overlap copied from end of previous buffer is not useful, so adjust return range to suit.
	// Skip first 4 zero samples at very start.
	if (samplePosition == 0)
	{
		int unused = SampleBufferOverlap * ChannelsCount();
		returnSamples -= unused;
		returnData += unused;
	}

	samplePosition += returnSamples;

	return std::tuple<float*, int>(returnData, returnSamples);
}

WavFileCursor::WavFileCursor(WavFileStreaming* pfile) : sampleData(pfile)
{
	const int bufferSize = 4096;
	buffer.resize(bufferSize);

	auto wave_data_bytes = bufferSize * sampleData->waveheader.wBitsPerSample / 8;
	conversionBuffer.resize(wave_data_bytes);

	if (sampleData->m_sampler_data.cSampleLoops > 0)
	{
		loopEndMarker = sampleData->m_sampler_data.Loops[0].dwEnd * ChannelsCount();
	}

	myfile.open(toOperatingSystemFilename(sampleData->filename), ios_base::in | ios_base::binary);
}

void WavFileCursor::Reset()
{
	// put some zeros in buffer 'before' sample start.
	lastSentSampleIndex = 0;
	for (auto i = 0; i < SampleBufferOverlap * ChannelsCount() * 2; ++i)
		buffer[i] = 0.0f;

	myfile.seekg(sampleData->wavedata_offset);
	samplePosition = 0;
}
