#include "stdafx.h"
#include "Logger.h"


#include <log4cpp/Category.hh>
#include <log4cpp/PropertyConfigurator.hh>

Logger::Logger()
{
	std::string initFileName = "log4cpp.properties";

	try
	{
		log4cpp::PropertyConfigurator::configure(initFileName);
		LogInfo("********* start logging... *********");
	}
	catch (log4cpp::ConfigureFailure& logExc)
	{
	}
}


Logger::~Logger()
{
	LogInfo("********* end logging... *********");
	log4cpp::Category::shutdown();
}

void Logger::LogDebug(string strDebug)
{
	log4cpp::Category& GeneralLogger = 
		log4cpp::Category::getInstance(std::string("GeneralLogger"));
	if(GeneralLogger.isDebugEnabled())
		GeneralLogger.debug(strDebug);
}

void Logger::LogInfo(string strInfo)
{
	log4cpp::Category& GeneralLogger = 
		log4cpp::Category::getInstance(std::string("GeneralLogger"));
	if(GeneralLogger.isInfoEnabled())
		GeneralLogger.info(strInfo);
}

void Logger::LogWarn(string strWarn)
{
	log4cpp::Category& GeneralLogger = 
		log4cpp::Category::getInstance(std::string("GeneralLogger"));
	if(GeneralLogger.isWarnEnabled())
		GeneralLogger.warn(strWarn);
}

void Logger::LogError(string strError)
{
	log4cpp::Category& GeneralLogger = 
		log4cpp::Category::getInstance(std::string("GeneralLogger"));
	if(GeneralLogger.isErrorEnabled())
		GeneralLogger.error(strError);
}


