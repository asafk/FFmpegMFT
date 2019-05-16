#pragma once
#include <string>
#include <log4cpp/Category.hh>
#include <log4cpp/PropertyConfigurator.hh>

using namespace std;

class Logger
{
private:
	Logger()
	{
		std::string initFileName = "FFmpegMFT.dll.properties";

		try
		{
			log4cpp::PropertyConfigurator::configure(initFileName);
			LogInfo("********* start logging... *********");
		}
		catch (log4cpp::ConfigureFailure& logExc)
		{
			//Log config not found, not using logging facilities
		}
	}

public:	
	~Logger()
	{
		LogInfo("********* end logging... *********");
		log4cpp::Category::shutdown();
	}

	const char* GeneralLoggerCategory = "GeneralLogger";
	const char* FFmpegLoggerCategory = "FFmpegLogger";

	static Logger& getInstance()
	{
		//"Meyers Singleton"
		//https://stackoverflow.com/a/43524389/3719543

		static Logger instance;
		return instance;
	}

	bool IsPiriorityLevelEnabled(int priorityLevel, const char* category)
	{
		log4cpp::Category& logger = 
		log4cpp::Category::getInstance(std::string(category));
		if(&logger != NULL && logger.isPriorityEnabled(priorityLevel))
		{
			return true;
		}
		return false;
	}

	template<typename... Args> 
	void Log(int level, const char* category, const char* stringFormat, Args... args)
	{
		log4cpp::Category& logger = 
		log4cpp::Category::getInstance(std::string(category));
		if(&logger != NULL && logger.isPriorityEnabled(level))
		{
			logger.log(level, stringFormat, args...);
		}
	}

	template<typename... Args> 
	void LogDebug(const char* stringFormat, Args... args)
	{
		log4cpp::Category& GeneralLogger = 
		log4cpp::Category::getInstance(std::string(GeneralLoggerCategory));
		if(GeneralLogger.isDebugEnabled())
		{
			GeneralLogger.debug(stringFormat, args...);
		}
	}

	template<typename... Args> 
	void LogInfo(const char* stringFormat, Args... args)
	{
		log4cpp::Category& GeneralLogger = 
		log4cpp::Category::getInstance(std::string(GeneralLoggerCategory));
		if(GeneralLogger.isInfoEnabled())
		{
			GeneralLogger.info(stringFormat, args...);					
		}
	}

	template<typename... Args> 
	void LogWarn(const char* stringFormat, Args... args)
	{
		log4cpp::Category& GeneralLogger = 
		log4cpp::Category::getInstance(std::string(GeneralLoggerCategory));
		if(GeneralLogger.isWarnEnabled())
		{
			GeneralLogger.warn(stringFormat, args...);				
		}
	}

	template<typename... Args> 
	void LogError(const char* stringFormat, Args... args)
	{
		log4cpp::Category& GeneralLogger = 
		log4cpp::Category::getInstance(std::string(GeneralLoggerCategory));
		if(GeneralLogger.isErrorEnabled())
		{
			GeneralLogger.error(stringFormat, args...);				
		}
	}
};