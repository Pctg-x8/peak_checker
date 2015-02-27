#pragma once

#include <Windows.h>
#include <mmsystem.h>
#include <cstdio>
#include <cstdint>
#include <string>
#include <array>
#include <limits>

#undef max

class wavReader
{
	template<typename T> using sample_pair = std::pair < T, T > ;

	std::array<float*, 2> bufferNormalized;
	std::uint64_t samples;
public:
	using sample_unorm_value = sample_pair < float >;
	WAVEFORMATEX wfe;

	static wavReader* load(const std::wstring& str);
	virtual ~wavReader();

	std::uint64_t getSampleCount() const { return samples; }
	sample_unorm_value readSample(std::uint64_t sample);
};
