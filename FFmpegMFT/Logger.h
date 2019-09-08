#pragma once
#include <string>
#include <algorithm>
#include <log4cpp/Category.hh>
#include <log4cpp/PropertyConfigurator.hh>

using namespace std;

extern "C"
{
	#include <libavutil/log.h>
}

static void log_callback(void* ptr, int level, const char* fmt, va_list vargs);
static log4cpp::threading::Mutex g_mutex;

class Logger
{
private:
	Logger()
	{
		std::string initFileName = "FFmpegMFT.dll.properties";

		try
		{
			log4cpp::PropertyConfigurator::configure(initFileName);
			LogInfo("********* start logging *********");
			av_log_set_callback(log_callback);
		}
		catch (log4cpp::ConfigureFailure& logExc)
		{
			//Log config not found, not using logging facilities
		}
	}

public:	
	~Logger()
	{
		av_log_set_callback(NULL);
		LogInfo("********** end logging **********");		
		log4cpp::Category::shutdown();
	}

	const char* FFmpegMFTCategory = "FFmpegMFT";
	const char* FFmpegLibCategory = "FFmpegLib";

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
		log4cpp::Category::getInstance(std::string(FFmpegMFTCategory));
		if(GeneralLogger.isDebugEnabled())
		{
			GeneralLogger.debug(stringFormat, args...);
		}
	}

	template<typename... Args> 
	void LogInfo(const char* stringFormat, Args... args)
	{
		log4cpp::Category& GeneralLogger = 
		log4cpp::Category::getInstance(std::string(FFmpegMFTCategory));
		if(GeneralLogger.isInfoEnabled())
		{
			GeneralLogger.info(stringFormat, args...);					
		}
	}

	template<typename... Args> 
	void LogWarn(const char* stringFormat, Args... args)
	{
		log4cpp::Category& GeneralLogger = 
		log4cpp::Category::getInstance(std::string(FFmpegMFTCategory));
		if(GeneralLogger.isWarnEnabled())
		{
			GeneralLogger.warn(stringFormat, args...);				
		}
	}

	template<typename... Args> 
	void LogError(const char* stringFormat, Args... args)
	{
		log4cpp::Category& GeneralLogger = 
		log4cpp::Category::getInstance(std::string(FFmpegMFTCategory));
		if(GeneralLogger.isErrorEnabled())
		{
			GeneralLogger.error(stringFormat, args...);				
		}
	}
};

static void log_callback(void* ptr, int level, const char* fmt, va_list vargs)
{
	try
	{
		log4cpp::threading::MSScopedLock guardMutex(g_mutex);

		int normalLevel = log4cpp::Priority::NOTSET;
		switch(level)
		{
		case AV_LOG_PANIC:
		case AV_LOG_FATAL:
		case AV_LOG_ERROR:
			normalLevel = log4cpp::Priority::ERROR;
			break;
		case AV_LOG_WARNING:
			normalLevel = log4cpp::Priority::WARN;
			break;
		case AV_LOG_INFO:
		case AV_LOG_VERBOSE:
			normalLevel = log4cpp::Priority::INFO;
			break;		
		case AV_LOG_DEBUG:
		case AV_LOG_TRACE:
			normalLevel = log4cpp::Priority::DEBUG;
			break;
		default:		
			break;
		}

		if(Logger::getInstance().IsPiriorityLevelEnabled(normalLevel, Logger::getInstance().FFmpegLibCategory))
		{
			const int size = 512;
			char dbg_out[size];
			if(strlen(fmt) <= size - 32){
				vsprintf_s(dbg_out, fmt, vargs);

				std::string str("FFmpeg: ");
				str.append(dbg_out);

				str.erase(std::remove(str.begin(), str.end(), '\n'), str.end());
				Logger::getInstance().Log(normalLevel, Logger::getInstance().FFmpegLibCategory, str.c_str());
			}
		}
	}
	catch (...)
	{
	}	
}