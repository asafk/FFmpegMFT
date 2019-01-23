#pragma once
#include <string>

class GeneralLogger
{
public:
	GeneralLogger();
	~GeneralLogger();

	void LogDebug(std::basic_string<TCHAR> strDebug);
	//void LogInfo(std::basic_string<TCHAR> strInfo);
	//void LogWarn(std::basic_string<TCHAR>  strWarn);
	//void LogError(std::basic_string<TCHAR> strError);
};

