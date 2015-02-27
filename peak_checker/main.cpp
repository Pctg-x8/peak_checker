#include <iostream>

#include "fileList.h"
#include "wavReader.h"

#include <vector>
#include <tuple>
#include <fstream>
#include <iomanip>
#include <map>

using float_limits = std::numeric_limits < float > ;

struct db_value
{
	float value;

	friend std::wostream& operator<<(std::wostream& ost, db_value dv)
	{
		dv.write(ost);
		return ost;
	}
public:
	db_value(float v) : value(v) { }

	std::wstring get_str(){ return value == 0.0 ? L"inf" : std::to_wstring(std::log10(value) * 20.0); }
	void write(std::wostream& ost) 
	{ 
		if(value == 0.0) ost << L"inf db";
		else ost << std::fixed << std::setprecision(2) << std::showpos << (std::log10(value) * 20.0) << L" db";
	}
};
struct group
{
	std::uint64_t validCount;
	wavReader::sample_unorm_value peakMax, peakMin, peakTotal;
	float monoPeakMax, monoPeakMin, monoPeakTotal;
	std::wstring file_max, file_min;

	group()
		: peakMax(0.0, 0.0), peakMin(float_limits::max(), float_limits::max()), peakTotal(0.0, 0.0),
		monoPeakMax(0.0), monoPeakMin(float_limits::max()), monoPeakTotal(0.0), file_max(L""), file_min(L""), validCount(0)
	{ }

	void printStatistics(std::wostream& ost, const std::wstring& group_name) const
	{
		auto peakAvg = std::make_pair(peakTotal.first / validCount, peakTotal.second / validCount);
		auto monoPeakAvg = monoPeakTotal / validCount;
		auto peakMed = std::make_pair((peakMax.first + peakMin.first) / 2.0, (peakMax.second + peakMin.second) / 2.0);
		auto monoPeakMed = (monoPeakMax + monoPeakMin) / 2.0;
		ost << L"Statistics[" << group_name << L"]:" << std::endl;
		ost << L"  Available:" << validCount << std::endl;
		ost << L"  Max Peaks:L=" << db_value(peakMax.first) << L"/R=" << db_value(peakMax.second) << L"/M=" << db_value(monoPeakMax) << L"(" << file_max << L")" << std::endl;
		ost << L"  Min Peaks:L=" << db_value(peakMin.first) << L"/R=" << db_value(peakMin.second) << L"/M=" << db_value(monoPeakMin) << L"(" << file_min << L")" << std::endl;
		ost << L"  Avg Peaks:L=" << db_value(peakAvg.first) << L"/R=" << db_value(peakAvg.second) << L"/M=" << db_value(monoPeakAvg) << std::endl;
		ost << L"  Med Peaks:L=" << db_value(peakMed.first) << L"/R=" << db_value(peakMed.second) << L"/M=" << db_value(monoPeakMed) << std::endl;
	}
};

wavReader::sample_unorm_value operator+(const wavReader::sample_unorm_value& v1, const wavReader::sample_unorm_value& v2)
{
	return wavReader::sample_unorm_value(v1.first + v2.first, v1.second + v2.second);
}

int wmain(int argc, wchar_t** argv)
{
	std::wstring dir_find = L"";

	if(argc > 1) dir_find = std::wstring(argv[1]); else
	{
		wchar_t buffer[MAX_PATH] = { };
		GetCurrentDirectory(MAX_PATH, buffer);
		dir_find = buffer;
	}
	std::wcout << L"BMS wav checker" << std::endl;
	std::wcout << L"checking directory " << dir_find << std::endl;

	std::map<std::wstring, group> groups;
	groups.insert(std::make_pair(L"", group()));
	auto gg = group();
	auto files = fileList::listDirectory(dir_find);
	// filename, bitdepth, samplerate, peaks, monopeaks, toolong, clipped
	std::vector<std::tuple<std::wstring, std::uint8_t, std::uint32_t, wavReader::sample_unorm_value, float, bool, bool>> measuredList;
	std::wcout << std::endl;
	for(const auto& f : files)
	{
		std::wcout << "  parsing " << f.first << L"...";
		auto pWavReader = wavReader::load(f.second);
		if(!pWavReader) std::wcout << L"Loading failed" << std::endl;
		else
		{
			std::wstring group_name = L"";
			if(f.first.find(L" - ") != f.first.npos)
			{
				// Edison / Slicex出力向けのグルーピング
				group_name = f.first.substr(0, f.first.find(L" - "));
			}
			if(groups.find(group_name) == std::end(groups)) groups.insert(std::make_pair(group_name, group()));
			auto& g = groups[group_name];

			std::wcout << std::endl << L"    ";
			std::wcout << pWavReader->wfe.wBitsPerSample << L"bit " << pWavReader->wfe.nSamplesPerSec << "Hz/s "
				<< (pWavReader->wfe.nChannels == 1 ? L"Mono" : L"Stereo") << L" Peak:";
			wavReader::sample_unorm_value peaks(0.0, 0.0);
			for(int i = 0; i < pWavReader->getSampleCount(); i++)
			{
				auto sample = pWavReader->readSample(i);
				if(abs(sample.first) > peaks.first) peaks.first = abs(sample.first);
				if(abs(sample.second) > peaks.second) peaks.second = abs(sample.second);
			}
			auto monoPeak = (peaks.first + peaks.second) / 2.0;

			// add group
			g.monoPeakTotal += monoPeak;
			if(g.monoPeakMax < monoPeak) { g.file_max = f.first; g.monoPeakMax = monoPeak; }
			if(g.monoPeakMin > monoPeak) { g.file_min = f.first; g.monoPeakMin = monoPeak; }
			g.peakTotal = g.peakTotal + peaks;
			if(g.peakMax.first < peaks.first) g.peakMax.first = peaks.first;
			if(g.peakMax.second < peaks.second) g.peakMax.second = peaks.second;
			if(g.peakMin.first > peaks.first) g.peakMin.first = peaks.first;
			if(g.peakMin.second > peaks.second) g.peakMin.second = peaks.second;
			g.validCount++;

			// add global group
			gg.monoPeakTotal += monoPeak;
			if(gg.monoPeakMax < monoPeak) { gg.file_max = f.first; gg.monoPeakMax = monoPeak; }
			if(gg.monoPeakMin > monoPeak) { gg.file_min = f.first; gg.monoPeakMin = monoPeak; }
			gg.peakTotal = gg.peakTotal + peaks;
			if(gg.peakMax.first < peaks.first) gg.peakMax.first = peaks.first;
			if(gg.peakMax.second < peaks.second) gg.peakMax.second = peaks.second;
			if(gg.peakMin.first > peaks.first) gg.peakMin.first = peaks.first;
			if(gg.peakMin.second > peaks.second) gg.peakMin.second = peaks.second;
			gg.validCount++;

			std::wcout << "L="; if(peaks.first == 0.0) std::wcout << L"inf db"; else std::wcout << (std::log10(peaks.first) * 20.0) << L" db";
			std::wcout << "/R="; if(peaks.second == 0.0) std::wcout << L"inf db"; else std::wcout << (std::log10(peaks.second) * 20.0) << L" db";
			std::wcout << "/M="; if(monoPeak == 0.0) std::wcout << L"inf db"; else std::wcout << (std::log10(monoPeak) * 20.0) << L" db";
			if(pWavReader->getSampleCount() > pWavReader->wfe.nSamplesPerSec * 60)
			{
				std::wcout << L" [Too long]";
			}
			if(peaks.first > 1.0 || peaks.second > 1.0)
			{
				std::wcout << L" [Clipped]";
			}
			std::wcout << std::endl;

			measuredList.push_back(std::make_tuple(f.first, pWavReader->wfe.wBitsPerSample, pWavReader->wfe.nSamplesPerSec, peaks, monoPeak,
				pWavReader->getSampleCount() > pWavReader->wfe.nSamplesPerSec * 60, peaks.first > 1.0 || peaks.second > 1.0));

			delete pWavReader;
		}
	}
	gg.printStatistics(std::wcout, L"Global Group");

	std::wcout << std::endl << L"Writing statistics...";
	std::wofstream fs(L"stats.log");
	
	for(const auto& t : measuredList)
	{
		fs << std::get<0>(t) << L":" << std::get<1>(t) << L"bits " << std::get<2>(t) << L"Hz/s Peak:L=";
		if(std::get<3>(t).first == 0.0) fs << L"inf"; else fs << std::fixed << std::setprecision(2) << (std::log10(std::get<3>(t).first) * 20.0); fs << L" db";
		fs << L"/R=";
		if(std::get<3>(t).second == 0.0) fs << L"inf"; else fs << std::fixed << std::setprecision(2) << (std::log10(std::get<3>(t).second) * 20.0); fs << L" db";
		fs << L"/M=";
		if(std::get<4>(t) == 0.0) fs << L"inf"; else fs << std::fixed << std::setprecision(2) << (std::log10(std::get<4>(t)) * 20.0); fs << L" db";
		if(std::get<5>(t)) fs << L" [Too long]";
		if(std::get<6>(t)) fs << L" [Clipped]";
		fs << std::endl;
	}

	gg.printStatistics(fs, L"Global Group");
	for(const auto& g : groups) g.second.printStatistics(fs, g.first.empty() ? L"Ungrouped" : g.first);

	std::wcout << std::endl << L"Thank you for using" << std::endl;
	return 0;
}

