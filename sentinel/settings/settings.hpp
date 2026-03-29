/*
	This header file was auto-generated from a .hson template
	WARNING: Do not make manual modifications, as they may be overwritten.
*/
#pragma once
#include "JSL/modules/Parameters/Parameter.h"
#include "JSL/modules/Parameters/Describer.h"
#include "JSL/modules/FileIO/fileWriters.h"
#include <cstdlib>//for exit(0)
#include <string>
#include <vector>
class SettingsObject_System_Headless
{
	public:
		SettingsObject_System_Headless(){}
		SettingsObject_System_Headless(int argc,char** argv){Parse(argc,argv);}

		bool Active = false;
		std::string CortexName = "My Project";
		std::string AuthorName = "Me";
		void Parse(int argc, char** argv)
		{
			Active = JSL::Parameter<bool>(Active,"headless",argc,argv).Value();
			CortexName = JSL::Parameter<std::string>(CortexName,"headless-name",argc,argv).Value();
			AuthorName = JSL::Parameter<std::string>(AuthorName,"headless-author",argc,argv).Value();
		}
		void Configure(const std::string & configFile, std::string configDelimiter)
		{
			Active = JSL::Parameter<bool>(Active,"headless",configFile,configDelimiter).Value();
			CortexName = JSL::Parameter<std::string>(CortexName,"headless-name",configFile,configDelimiter).Value();
			AuthorName = JSL::Parameter<std::string>(AuthorName,"headless-author",configFile,configDelimiter).Value();
		}
		std::string ToText()
		{
			std::ostringstream s;
			s << "headless " << JSL::MakeString(Active) << "\n";
			s << "headless-name " << JSL::MakeString(CortexName) << "\n";
			s << "headless-author " << JSL::MakeString(AuthorName) << "\n";
			return s.str();
		}
		void Help(JSL::HelpMessages & help)
		{
			help.AddMessage("SettingsObject_System_Headless","headless",false,"Active","If true, runs the system in headless mode");
			help.AddMessage("SettingsObject_System_Headless","headless-name","My Project","CortexName","The default name given to a new cortex instantiation if not provided by the user");
			help.AddMessage("SettingsObject_System_Headless","headless-author","Me","AuthorName","The default author assigned to a new cortex instantiation if not provided by the user");
		}
		void GetDescription(std::string parameter,std::vector<JSL::ParameterDescription> & found)
		{
			JSL::ParameterDescription("Active","bool","headless",(bool)Active,(bool)false,"If true, runs the system in headless mode").Query(parameter,found);
			JSL::ParameterDescription("CortexName","std::string","headless-name",(std::string)CortexName,(std::string)"My Project","The default name given to a new cortex instantiation if not provided by the user").Query(parameter,found);
			JSL::ParameterDescription("AuthorName","std::string","headless-author",(std::string)AuthorName,(std::string)"Me","The default author assigned to a new cortex instantiation if not provided by the user").Query(parameter,found);
		}
};
class SettingsObject_System
{
	public:
		SettingsObject_System(){}
		SettingsObject_System(int argc,char** argv){Parse(argc,argv);}

		int DispatchDelay = 10;
		bool Verbose = false;
		bool Quiet = false;
		std::string TerminationFileName = "cortex_disable_message";
		SettingsObject_System_Headless Headless;
		void Parse(int argc, char** argv)
		{
			DispatchDelay = JSL::Parameter<int>(DispatchDelay,"delay",argc,argv).Value();
			Verbose = JSL::Parameter<bool>(Verbose,"v",argc,argv).Value();
			Quiet = JSL::Parameter<bool>(Quiet,"q",argc,argv).Value();
			TerminationFileName = JSL::Parameter<std::string>(TerminationFileName,"terminate",argc,argv).Value();
			Headless.Parse(argc,argv);
		}
		void Configure(const std::string & configFile, std::string configDelimiter)
		{
			DispatchDelay = JSL::Parameter<int>(DispatchDelay,"delay",configFile,configDelimiter).Value();
			Verbose = JSL::Parameter<bool>(Verbose,"v",configFile,configDelimiter).Value();
			Quiet = JSL::Parameter<bool>(Quiet,"q",configFile,configDelimiter).Value();
			TerminationFileName = JSL::Parameter<std::string>(TerminationFileName,"terminate",configFile,configDelimiter).Value();
			Headless.Configure(configFile,configDelimiter);
		}
		std::string ToText()
		{
			std::ostringstream s;
			s << "delay " << JSL::MakeString(DispatchDelay) << "\n";
			s << "v " << JSL::MakeString(Verbose) << "\n";
			s << "q " << JSL::MakeString(Quiet) << "\n";
			s << "terminate " << JSL::MakeString(TerminationFileName) << "\n";
			s << Headless.ToText();
			return s.str();
		}
		void Help(JSL::HelpMessages & help)
		{
			help.AddMessage("SettingsObject_System","delay",10,"DispatchDelay","The delay time (in ms) between detecting a filechange and dispatching the calls to the manager.");
			help.AddMessage("SettingsObject_System","v",false,"Verbose","Inlcudes debugging error messages. Overrides quiet");
			help.AddMessage("SettingsObject_System","q",false,"Quiet","Suppresses all outputs except errors.");
			help.AddMessage("SettingsObject_System","terminate","cortex_disable_message","TerminationFileName","If a file with this name appears in a watched directory, cortex will take this as a signal to exit. The file is deleted.");
			Headless.Help(help);
		}
		void GetDescription(std::string parameter,std::vector<JSL::ParameterDescription> & found)
		{
			JSL::ParameterDescription("DispatchDelay","int","delay",(int)DispatchDelay,(int)10,"The delay time (in ms) between detecting a filechange and dispatching the calls to the manager.").Query(parameter,found);
			JSL::ParameterDescription("Verbose","bool","v",(bool)Verbose,(bool)false,"Inlcudes debugging error messages. Overrides quiet").Query(parameter,found);
			JSL::ParameterDescription("Quiet","bool","q",(bool)Quiet,(bool)false,"Suppresses all outputs except errors.").Query(parameter,found);
			JSL::ParameterDescription("TerminationFileName","std::string","terminate",(std::string)TerminationFileName,(std::string)"cortex_disable_message","If a file with this name appears in a watched directory, cortex will take this as a signal to exit. The file is deleted.").Query(parameter,found);
			Headless.GetDescription(parameter,found);
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
		std::vector<std::string> IgnoredPatterns = {"*.git*","*.build*","*/libs*","*.cortex"};
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
		std::string ToText()
		{
			std::ostringstream s;
			s << "i " << JSL::MakeString(TargetDirectory) << "\n";
			s << "directory " << JSL::MakeString(OutputDirectory) << "\n";
			s << "watch " << JSL::MakeString(WatchedPatterns) << "\n";
			s << "ignore " << JSL::MakeString(IgnoredPatterns) << "\n";
			return s.str();
		}
		void Help(JSL::HelpMessages & help)
		{
			help.AddMessage("SettingsObject_Files","i",".","TargetDirectory","The target directory to launch the cortex process in. If no argument is provided, activates in the current working directory.");
			help.AddMessage("SettingsObject_Files","directory","compiled","OutputDirectory","The name of the output directory which will contain the compiled pdf");
			help.AddMessage("SettingsObject_Files","watch",(std::vector<std::string>){"*.tex","*.dat"},"WatchedPatterns","The file patterns included in the indexing process");
			help.AddMessage("SettingsObject_Files","ignore",(std::vector<std::string>){"*.git*","*.build*","*/libs*","*.cortex"},"IgnoredPatterns","Directory name patterns which are not watched for new files");
		}
		void GetDescription(std::string parameter,std::vector<JSL::ParameterDescription> & found)
		{
			JSL::ParameterDescription("TargetDirectory","std::string","i",(std::string)TargetDirectory,(std::string)".","The target directory to launch the cortex process in. If no argument is provided, activates in the current working directory.").Query(parameter,found);
			JSL::ParameterDescription("OutputDirectory","std::string","directory",(std::string)OutputDirectory,(std::string)"compiled","The name of the output directory which will contain the compiled pdf").Query(parameter,found);
			JSL::ParameterDescription("WatchedPatterns","std::vector<std::string>","watch",(std::vector<std::string>)WatchedPatterns,(std::vector<std::string>){"*.tex","*.dat"},"The file patterns included in the indexing process").Query(parameter,found);
			JSL::ParameterDescription("IgnoredPatterns","std::vector<std::string>","ignore",(std::vector<std::string>)IgnoredPatterns,(std::vector<std::string>){"*.git*","*.build*","*/libs*","*.cortex"},"Directory name patterns which are not watched for new files").Query(parameter,found);
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
		std::string ToText()
		{
			std::ostringstream s;
			s << System.ToText();
			s << Files.ToText();
			return s.str();
		}
		void Help(JSL::HelpMessages & help)
		{
			System.Help(help);
			Files.Help(help);
		}
		std::vector<JSL::ParameterDescription>  GetDescription(std::string parameter)
		{
			std::vector<JSL::ParameterDescription> found;
			
			System.GetDescription(parameter,found);
			Files.GetDescription(parameter,found);
			return found;
		}
		void SaveConfig(std::string file)
		{
			auto contents = ToText();

			JSL::writeStringToFile(file,contents,std::ios::out);
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
extern SettingsObject Settings;