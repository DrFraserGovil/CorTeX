/*
	This header file was auto-generated from a .hson template
	WARNING: Do not make manual modifications, as they may be overwritten.
*/
#pragma once
#include "JSL/modules/Parameters/Parameter.h"
#include <cstdlib>//for exit(0)
#include <string>
#include <vector>
class SettingsObject_System
{
	public:
		SettingsObject_System(){}
		SettingsObject_System(int argc,char** argv){Parse(argc,argv);}

		int PollingDelay = 10;
		bool Verbose = false;
		bool Quiet = false;
		std::string TerminationFileName = "foamtex_disable_message";
		bool ForceCreate = false;
		void Parse(int argc, char** argv)
		{
			PollingDelay = JSL::Parameter<int>(PollingDelay,"delay",argc,argv).Value();
			Verbose = JSL::Parameter<bool>(Verbose,"v",argc,argv).Value();
			Quiet = JSL::Parameter<bool>(Quiet,"q",argc,argv).Value();
			TerminationFileName = JSL::Parameter<std::string>(TerminationFileName,"terminate",argc,argv).Value();
			ForceCreate = JSL::Parameter<bool>(ForceCreate,"f",argc,argv).Value();
		}
		void Configure(const std::string & configFile, std::string configDelimiter)
		{
			PollingDelay = JSL::Parameter<int>(PollingDelay,"delay",configFile,configDelimiter).Value();
			Verbose = JSL::Parameter<bool>(Verbose,"v",configFile,configDelimiter).Value();
			Quiet = JSL::Parameter<bool>(Quiet,"q",configFile,configDelimiter).Value();
			TerminationFileName = JSL::Parameter<std::string>(TerminationFileName,"terminate",configFile,configDelimiter).Value();
			ForceCreate = JSL::Parameter<bool>(ForceCreate,"f",configFile,configDelimiter).Value();
		}
		void Help(JSL::HelpMessages & help)
		{
			help.AddMessage("SettingsObject_System","delay",10,"PollingDelay","The delay time between polls (in ms)");
			help.AddMessage("SettingsObject_System","v",false,"Verbose","Inlcudes debugging error messages. Overrides quiet");
			help.AddMessage("SettingsObject_System","q",false,"Quiet","Suppresses all outputs except errors.");
			help.AddMessage("SettingsObject_System","terminate","foamtex_disable_message","TerminationFileName","If a file with this name appears in a watched directory, foamtex will take this as a signal to exit. The file is deleted.");
			help.AddMessage("SettingsObject_System","f",false,"ForceCreate","If true, automatically creates a foamtex index in the current directory");
		}
};
class SettingsObject_Files
{
	public:
		SettingsObject_Files(){}
		SettingsObject_Files(int argc,char** argv){Parse(argc,argv);}

		std::string TargetDirectory = ".";
		std::string OutputDirectory = "compiled";
		std::vector<std::string> WatchedPatterns = {"*.tex","*.dat"};
		std::vector<std::string> IgnoredPatterns = {"*.git*","*.build*","*/libs*"};
		void Parse(int argc, char** argv)
		{
			TargetDirectory = JSL::Parameter<std::string>(TargetDirectory,"i",argc,argv).Value();
			OutputDirectory = JSL::Parameter<std::string>(OutputDirectory,"directory",argc,argv).Value();
			WatchedPatterns = JSL::Parameter<std::vector<std::string>>(WatchedPatterns,"watch",argc,argv).Value();
			IgnoredPatterns = JSL::Parameter<std::vector<std::string>>(IgnoredPatterns,"ignore",argc,argv).Value();
		}
		void Configure(const std::string & configFile, std::string configDelimiter)
		{
			TargetDirectory = JSL::Parameter<std::string>(TargetDirectory,"i",configFile,configDelimiter).Value();
			OutputDirectory = JSL::Parameter<std::string>(OutputDirectory,"directory",configFile,configDelimiter).Value();
			WatchedPatterns = JSL::Parameter<std::vector<std::string>>(WatchedPatterns,"watch",configFile,configDelimiter).Value();
			IgnoredPatterns = JSL::Parameter<std::vector<std::string>>(IgnoredPatterns,"ignore",configFile,configDelimiter).Value();
		}
		void Help(JSL::HelpMessages & help)
		{
			help.AddMessage("SettingsObject_Files","i",".","TargetDirectory","The target directory to launch the foamtex process in. If no argument is provided, activates in the current working directory.");
			help.AddMessage("SettingsObject_Files","directory","compiled","OutputDirectory","The name of the output directory which will contain the compiled pdfs");
			help.AddMessage("SettingsObject_Files","watch",(std::vector<std::string>){"*.tex","*.dat"},"WatchedPatterns","The file patterns included in the indexing process");
			help.AddMessage("SettingsObject_Files","ignore",(std::vector<std::string>){"*.git*","*.build*","*/libs*"},"IgnoredPatterns","Directory name patterns which are not watched for new files");
		}
};
class SettingsObject
{
	public:
		SettingsObject(){}
		SettingsObject(int argc,char** argv){Parse(argc,argv);}

		SettingsObject_System System;
		SettingsObject_Files Files;
		void Parse(int argc, char** argv)
		{
			//Special handling to trigger configuration or help
			std::string NULLFILE = "__none__";
			JSL::Parameter<std::string> ConfigureFile(JSL::internal::NULLFILE,"config",argc,argv);
			JSL::Parameter<std::string> ConfigureDelimiter(" ","config-delimiter",argc,argv);
			bool HelpRequested = JSL::Toggle("help",argc,argv).Value() || JSL::Toggle("h",argc,argv).Value();
			if (HelpRequested)
			{
				MasterHelp();
			}
			if (ConfigureFile.Value() != NULLFILE)
			{
				Configure(ConfigureFile.Value(),ConfigureDelimiter.Value());
			}
			System.Parse(argc,argv);
			Files.Parse(argc,argv);
		}
		void Configure(const std::string & configFile, std::string configDelimiter)
		{
			System.Configure(configFile,configDelimiter);
			Files.Configure(configFile,configDelimiter);
		}
		void Help(JSL::HelpMessages & help)
		{
			System.Help(help);
			Files.Help(help);
		}
	private:
		void MasterHelp()
		{
			JSL::HelpMessages help;
			help.AddMessage("SettingsObject","config","__none__","ConfigureFile","When not equal to '__none__', the system will attempt to read this file in as a configuration file.\nConfiguration files work the same as command line arguments, each line should contain a single flag and a value\nIMPORTANT: Flags in config files omit the '-'");
			help.AddMessage("SettingsObject","configure-delimiter"," ","ConfigureDelimiter","The string which separates the flag from the values in the config file.\nOnly the first instance of the flag is counted, subsequent occurrences are ignored.");
			help.AddMessage("SettingsObject","h, --help",false,"Help","When true, activates the help page, then exits");
			Help(help);
			help.print();
			exit(0);
		}
};