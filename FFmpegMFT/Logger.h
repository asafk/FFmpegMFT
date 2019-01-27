#pragma once
#include <string>

using namespace std;

class Logger
{
public:
	Logger();
	~Logger();

	void LogDebug(string strDebug);
	void LogInfo(string strInfo);
	void LogWarn(string strWarn);
	void LogError(string strError);
};

