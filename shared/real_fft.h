/*
#include "../shared/real_fft.h"
*/
#ifndef REAL_FFT_H_INCLUDED
#define REAL_FFT_H_INCLUDED

#include <vector>

void realft2(float data[], unsigned int n, int isign); // normal C++ indexing 0 - (n-1)
void realft(float data[], unsigned int n, int isign); // 1 - n

class WindowedFft
{
	std::vector<float> hanningWindow;
	int fftSize;
	float windowSum; // for scaling result.

public:
	WindowedFft(int pFftSize);

	void ComputeMagnitudeSpectrum(const float* samples, std::vector<float>& magnitudeSpectrum);

};


class SineTables
{
	//float Forward1024[32];
	//float Reverse1024[32];
	std::vector<float> Forward1024;
	std::vector<float> Reverse1024;

public:
	SineTables();
	void InitTable(std::vector<float>& table, int sign );

	const float* GetTable( int fftSize, int fftSign );
};

#endif