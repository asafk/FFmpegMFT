#include "stdafx.h"
#include "GeneralLogger.h"
#include <log4cplus/logger.h>
#include <log4cplus/configurator.h>
#include <log4cplus/helpers/loglog.h>
#include <log4cplus/helpers/stringhelper.h>
#include <log4cplus/helpers/sleep.h>
#include <log4cplus/helpers/fileinfo.h>
#include <log4cplus/loggingmacros.h>

using namespace log4cplus;
using namespace log4cplus::helpers;

GeneralLogger::GeneralLogger()
{
	log4cplus::initialize ();

	log4cplus::ConfigureAndWatchThread configureThread(LOG4CPLUS_TEXT("log4cplus.properties"), 5 * 1000);

	Logger root = Logger::getRoot();

	LOG4CPLUS_WARN(root,LOG4CPLUS_TEXT("stam"));

	LogLog::getLogLog()->setQuietMode(true);
}


GeneralLogger::~GeneralLogger()
{
}

void GeneralLogger::LogDebug(std::basic_string<TCHAR> strDebug)
{
	LogLog::getLogLog()->debug((strDebug));
}


