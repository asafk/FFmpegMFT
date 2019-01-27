#pragma once
#include <string>

using namespace std;

class Logger
{
private:
	Logger();

public:	
	~Logger();

	static Logger& getInstance()
	{
		//"Meyers Singleton"
		//https://stackoverflow.com/a/43524389/3719543

		static Logger instance;
		return instance;
	}

	void LogDebug(string strDebug);
	void LogInfo(string strInfo);
	void LogWarn(string strWarn);
	void LogError(string strError);
};

