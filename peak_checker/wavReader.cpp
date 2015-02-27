#include "wavReader.h"

#pragma comment(lib, "winmm")

struct int24_t
{
	signed int value : 24;

	int24_t(std::uint8_t vp[3]) { value = vp[0] | (vp[1] << 8) | (vp[2] << 16); }
};

wavReader* wavReader::load(const std::wstring& str)
{
	if(str.empty()) return nullptr;

	auto pReader = new wavReader;
	if(!pReader) return nullptr;

	MMRESULT res;
	MMIOINFO minfo = { };
	auto handle = mmioOpen(const_cast<LPWSTR>(str.c_str()), &minfo, MMIO_READ);
	if(!handle) return nullptr;

	// enter riff chunk
	MMCKINFO ckRiff;
	ckRiff.fccType = MAKEFOURCC('W', 'A', 'V', 'E');
	res = mmioDescend(handle, &ckRiff, nullptr, MMIO_FINDRIFF);
	if(res != MMSYSERR_NOERROR) { mmioClose(handle, 0); return nullptr; }

	// fetch format
	MMCKINFO ckFormat;
	ckFormat.ckid = MAKEFOURCC('f', 'm', 't', ' ');
	res = mmioDescend(handle, &ckFormat, &ckRiff, MMIO_FINDCHUNK);
	if(res != MMSYSERR_NOERROR) { mmioClose(handle, 0); return nullptr; }
	auto ckFormatSize = ckFormat.cksize;
	auto ckFormatReadSize = mmioRead(handle, (HPSTR)&pReader->wfe, ckFormatSize);
	if(ckFormatSize != ckFormatReadSize) { mmioClose(handle, 0); return nullptr; }
	mmioAscend(handle, &ckFormat, 0);

	// fetch data
	MMCKINFO ckData;
	ckData.ckid = MAKEFOURCC('d', 'a', 't', 'a');
	res = mmioDescend(handle, &ckData, &ckRiff, MMIO_FINDCHUNK);
	if(res != MMSYSERR_NOERROR) { mmioClose(handle, 0); return nullptr; }
	auto ckDataSize = ckData.cksize;
	unsigned char* bufferTemp = new unsigned char[ckDataSize];
	if(!bufferTemp) { mmioClose(handle, 0); return nullptr; }
	auto ckDataReadSize = mmioRead(handle, (HPSTR)bufferTemp, ckDataSize);
	if(ckDataReadSize != ckDataSize) { free(bufferTemp); mmioClose(handle, 0); return nullptr; }
	pReader->samples = ckDataSize / pReader->wfe.nChannels / (pReader->wfe.wBitsPerSample >> 3);
	pReader->bufferNormalized[0] = new float[pReader->samples];
	pReader->bufferNormalized[1] = new float[pReader->samples];
	mmioAscend(handle, &ckData, 0);

	// end
	mmioAscend(handle, &ckRiff, 0);
	mmioClose(handle, 0);

	// translate formats
	switch(pReader->wfe.wBitsPerSample)
	{
	case 8:
		// unsigned char(0-255)
		for(int i = 0; i < pReader->samples; i++)
		{
			switch(pReader->wfe.nChannels)
			{
			case 1:
				// mono -> duplicate
				pReader->bufferNormalized[0][i] = pReader->bufferNormalized[1][i] = (bufferTemp[i] / 255.0) - 0.5;
				break;
			case 2:
				// stereo
				pReader->bufferNormalized[0][i] = (bufferTemp[i * 2 + 0] / 255.0) - 0.5;
				pReader->bufferNormalized[1][i] = (bufferTemp[i * 2 + 1] / 255.0) - 0.5;
				break;
			}
		}
		break;
	case 16:
		// signed short(-32767-32767)
		for(int i = 0; i < pReader->samples; i++)
		{
			switch(pReader->wfe.nChannels)
			{
			case 1:
				// mono -> duplicate
				pReader->bufferNormalized[0][i] = pReader->bufferNormalized[1][i] = ((signed short*)bufferTemp)[i] / 32767.0;
				break;
			case 2:
				// stereo
				pReader->bufferNormalized[0][i] = ((signed short*)bufferTemp)[i * 2 + 0] / 32767.0;
				pReader->bufferNormalized[1][i] = ((signed short*)bufferTemp)[i * 2 + 1] / 32767.0;
				break;
			}
		}
		break;
	case 24:
		// signed int24_t
		for(int i = 0; i < pReader->samples; i++)
		{
			std::uint8_t* vp;
			static double max_value = (1 << 23) - 1.0;
			switch(pReader->wfe.nChannels)
			{
			case 1:
				// mono -> duplicate
				vp = bufferTemp + (i * 3);
				pReader->bufferNormalized[0][i] = pReader->bufferNormalized[1][i] = int24_t(vp).value / max_value;
				break;
			case 2:
				// stereo
				vp = bufferTemp + ((i * 2 + 0) * 3);
				pReader->bufferNormalized[0][i] = int24_t(vp).value / max_value;
				vp = bufferTemp + ((i * 2 + 1) * 3);
				pReader->bufferNormalized[1][i] = int24_t(vp).value / max_value;
				break;
			}
		}
		break;
	case 32:
		// float(-fmax-fmax)
		for(int i = 0; i < pReader->samples; i++)
		{
			switch(pReader->wfe.nChannels)
			{
			case 1:
				// mono -> duplicate
				pReader->bufferNormalized[0][i] = pReader->bufferNormalized[1][i] = ((float*)bufferTemp)[i];
				break;
			case 2:
				// stereo
				pReader->bufferNormalized[0][i] = ((float*)bufferTemp)[i * 2 + 0];
				pReader->bufferNormalized[1][i] = ((float*)bufferTemp)[i * 2 + 1];
				break;
			}
		}
		break;
	case 64:
		// double(-dmax-dmax)
		for(int i = 0; i < pReader->samples; i++)
		{
			switch(pReader->wfe.nChannels)
			{
			case 1:
				// mono -> duplicate
				pReader->bufferNormalized[0][i] = pReader->bufferNormalized[1][i] = ((double*)bufferTemp)[i];
				break;
			case 2:
				// stereo
				pReader->bufferNormalized[0][i] = ((double*)bufferTemp)[i * 2 + 0];
				pReader->bufferNormalized[1][i] = ((double*)bufferTemp)[i * 2 + 1];
				break;
			}
		}
		break;
	default:
		delete[] pReader->bufferNormalized[0];
		delete[] pReader->bufferNormalized[1];
		delete[] bufferTemp;
		return nullptr;
	}

	delete[] bufferTemp;

	return pReader;
}
wavReader::~wavReader()
{
	for(auto& b : this->bufferNormalized)
	{
		if(b) { delete[] b; b = nullptr; }
	}
}

wavReader::sample_unorm_value wavReader::readSample(std::uint64_t sample)
{
	if(sample >= this->samples) return std::make_pair(0.0, 0.0);
	if(!this->bufferNormalized[0]) return std::make_pair(0.0, 0.0);
	return std::make_pair(this->bufferNormalized[0][sample], this->bufferNormalized[1][sample]);
}
