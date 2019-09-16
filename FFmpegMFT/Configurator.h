#pragma once
#include <string>
#include <fstream>
#include <map>
#include "Logger.h"

typedef std::map<std::string, std::string> STRING_MAP;
typedef STRING_MAP::const_iterator CONST_ITER_STRING_MAP;

// config object based on the Properties class from log4cpp
// 
class Configurator
{
private:
	Configurator()
	{
		const std::string initFile(string(PROPERTIES_FILE_STR));
		
		try
		{			
			init(initFile);			
		}
		catch (...)
		{
			Logger::getInstance().LogError("Failed to init the configuration");
		}
	}

	STRING_MAP m_parameters;

	 std::string trim(const std::string& s) {
        static const char* whiteSpace = " \t\r\n";

        // test for null string
        if(s.empty())
            return s;

        // find first non-space character
        std::string::size_type b = s.find_first_not_of(whiteSpace);
        if(b == std::string::npos) // No non-spaces
            return "";

        // find last non-space character
        std::string::size_type e = s.find_last_not_of(whiteSpace);

        // return the remaining characters
        return std::string(s, b, e - b + 1);
    }

public:	
	~Configurator()
	{		
	}
	
	static Configurator& getInstance()
	{
		//"Meyers Singleton"
		//https://stackoverflow.com/a/43524389/3719543

		static Configurator instance;
		return instance;
	}

	void init(const std::string& initFileName)
	{
		std::ifstream in(initFileName.c_str());
		m_parameters.clear();

        std::string fullLine, command;
        std::string leftSide, rightSide;
        char line[256];
        std::string::size_type length;
        bool partiallyRead(false);	// for strings longer than 256 chars

        while (in) {
        	if (in.getline(line, 256) || !in.bad()) {
        		// either string is read fully or only partially (logical but not read/write error)
        		if (partiallyRead)
        			fullLine.append(line);
        		else
        			fullLine = line;
        		partiallyRead = (in.fail() && !in.bad());
        		if (partiallyRead && !in.eof()) {
        			in.clear(in.rdstate() & ~std::ios::failbit);
        			continue; // to get full line
        		}
        	} else {
       			break;
        	}
            /* if the line contains a # then it is a comment
               if we find it anywhere other than the beginning, then we assume 
               there is a command on that line, and it we don't find it at all
               we assume there is a command on the line (we test for valid 
               command later) if neither is true, we continue with the next line
            */
            length = fullLine.find('#');
            if (length == std::string::npos) {
                command = fullLine;
            } else if (length > 0) {
                command = fullLine.substr(0, length);
            } else {
                continue;
            }

            // check the command and handle it
            length = command.find('=');
            if (length != std::string::npos) {
                leftSide = trim(command.substr(0, length));
                rightSide = trim(command.substr(length + 1, command.size() - length));
            } else {
                continue;
            }
        	
            /* handle the command by determining what object the left side
               refers to and setting the value given on the right
               ASSUMPTIONS:  
               1. first object given  on left side is "FFmpegMFT"
               2. all class names on right side are ignored because we
               probably cannot resolve them anyway.
            */

        	// strip off the "FFmpegMFT" and discard the rest ("log4cpp")
            length = leftSide.find('.');
            if (leftSide.substr(0, length) == "FFmpegMFT")
                leftSide = leftSide.substr(length + 1);
			else
				continue;            

        	Logger::getInstance().LogDebug("Add new parameter [%s = %s]", leftSide.c_str(), rightSide.c_str());
            // add to the map of properties
            m_parameters.insert(std::pair<std::string,std::string>(leftSide, rightSide));
        }
	}

	int getInt(const std::string& property, int defaultValue)
	{
		CONST_ITER_STRING_MAP key = m_parameters.find(property);
        return (key == m_parameters.end()) ? defaultValue : std::atoi((*key).second.c_str());
	}
	
    bool getBool(const std::string& property, bool defaultValue)
	{
		CONST_ITER_STRING_MAP key = m_parameters.find(property);
        return (key == m_parameters.end()) ? defaultValue : ((*key).second == "true");
	}
    std::string getString(const std::string& property, const char* defaultValue)
	{
		CONST_ITER_STRING_MAP key = m_parameters.find(property);
        return (key == m_parameters.end()) ? std::string(defaultValue) : (*key).second;
	}
	
};