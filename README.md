# FFmpegMFT
Description:
H.264 and H.265\HEVC decoder base on the FFmpeg library and implement using Microsoft MFT.(Media Foundation Transform)
Made by Asaf Kave

Capabilities:
 - Supporting both SW decoding and hardware acceleration 
 - H.264, Baseline, Main, and High profiles, up to level 5.1. (See ITU-T H.264 specification for details.)
 - H.265, Main, Main Still Picture, and Main10 profiles
 - Output format NV12, YV12, YUY2


Logging:
This is based on the Log4cpp, logging is disabled by default, to enable, add "FFmpegMFT.dll.properties" with Log4cpp confige, see below

	# log4cpp.properties

	log4cpp.rootCategory=DEBUG, rootAppender
	log4cpp.category.GeneralLogger=DEBUG, A1

	log4cpp.appender.rootAppender=ConsoleAppender
	log4cpp.appender.rootAppender.layout=PatternLayout
	log4cpp.appender.rootAppender.layout.ConversionPattern=%d [%p] [%t] %m%n 

	log4cpp.appender.A1=RollingFileAppender
	log4cpp.appender.A1.fileName=GeneralLogger.log
	log4cpp.appender.A1.threshold=DEBUG
	log4cpp.appender.A1.maxFileSize=5253125
	log4cpp.appender.A1.maxBackupIndex=1
	log4cpp.appender.A1.layout=PatternLayout
	log4cpp.appender.A1.layout.ConversionPattern=%d [%p] [%t] %m%n 
