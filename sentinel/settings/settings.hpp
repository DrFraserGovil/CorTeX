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
		void ParseLine(const std::vector<std::string> & linevec)
		{
			Active = JSL::Parameter<bool>(Active,"headless",linevec).Value();
			CortexName = JSL::Parameter<std::string>(CortexName,"headless-name",linevec).Value();
			AuthorName = JSL::Parameter<std::string>(AuthorName,"headless-author",linevec).Value();
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
			JSL::ParameterDescription("Active","bool","headless",Active,(bool)false,"If true, runs the system in headless mode").Query(parameter,found);
			JSL::ParameterDescription("CortexName","std::string","headless-name",CortexName,(std::string)"My Project","The default name given to a new cortex instantiation if not provided by the user").Query(parameter,found);
			JSL::ParameterDescription("AuthorName","std::string","headless-author",AuthorName,(std::string)"Me","The default author assigned to a new cortex instantiation if not provided by the user").Query(parameter,found);
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
		void ParseLine(const std::vector<std::string> & linevec)
		{
			DispatchDelay = JSL::Parameter<int>(DispatchDelay,"delay",linevec).Value();
			Verbose = JSL::Parameter<bool>(Verbose,"v",linevec).Value();
			Quiet = JSL::Parameter<bool>(Quiet,"q",linevec).Value();
			TerminationFileName = JSL::Parameter<std::string>(TerminationFileName,"terminate",linevec).Value();
			Headless.ParseLine(linevec);
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
			JSL::ParameterDescription("DispatchDelay","int","delay",DispatchDelay,(int)10,"The delay time (in ms) between detecting a filechange and dispatching the calls to the manager.").Query(parameter,found);
			JSL::ParameterDescription("Verbose","bool","v",Verbose,(bool)false,"Inlcudes debugging error messages. Overrides quiet").Query(parameter,found);
			JSL::ParameterDescription("Quiet","bool","q",Quiet,(bool)false,"Suppresses all outputs except errors.").Query(parameter,found);
			JSL::ParameterDescription("TerminationFileName","std::string","terminate",TerminationFileName,(std::string)"cortex_disable_message","If a file with this name appears in a watched directory, cortex will take this as a signal to exit. The file is deleted.").Query(parameter,found);
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
		std::string BuildDirectory = ".cortex/build";
		std::vector<std::string> WatchedPatterns = {"*.tex","*.dat"};
		std::vector<std::string> IgnoredPatterns = {"*.git*","*.build*","*.cortex","*sentinel*"};
		size_t StructureDelimiterRepeatCount = 3;
		void Parse(int argc, char** argv)
		{
			TargetDirectory = JSL::Parameter<std::string>(TargetDirectory,"i",argc,argv).Value();
			OutputDirectory = JSL::Parameter<std::string>(OutputDirectory,"directory",argc,argv).Value();
			BuildDirectory = JSL::Parameter<std::string>(BuildDirectory,"build",argc,argv).Value();
			WatchedPatterns = JSL::Parameter<std::vector<std::string>>(WatchedPatterns,"watch",argc,argv).Value();
			IgnoredPatterns = JSL::Parameter<std::vector<std::string>>(IgnoredPatterns,"ignore",argc,argv).Value();
			StructureDelimiterRepeatCount = JSL::Parameter<size_t>(StructureDelimiterRepeatCount,"delimiter-repeat",argc,argv).Value();
		}
		void Configure(const std::string & configFile, std::string configDelimiter)
		{
			TargetDirectory = JSL::Parameter<std::string>(TargetDirectory,"i",configFile,configDelimiter).Value();
			OutputDirectory = JSL::Parameter<std::string>(OutputDirectory,"directory",configFile,configDelimiter).Value();
			BuildDirectory = JSL::Parameter<std::string>(BuildDirectory,"build",configFile,configDelimiter).Value();
			WatchedPatterns = JSL::Parameter<std::vector<std::string>>(WatchedPatterns,"watch",configFile,configDelimiter).Value();
			IgnoredPatterns = JSL::Parameter<std::vector<std::string>>(IgnoredPatterns,"ignore",configFile,configDelimiter).Value();
			StructureDelimiterRepeatCount = JSL::Parameter<size_t>(StructureDelimiterRepeatCount,"delimiter-repeat",configFile,configDelimiter).Value();
		}
		void ParseLine(const std::vector<std::string> & linevec)
		{
			TargetDirectory = JSL::Parameter<std::string>(TargetDirectory,"i",linevec).Value();
			OutputDirectory = JSL::Parameter<std::string>(OutputDirectory,"directory",linevec).Value();
			BuildDirectory = JSL::Parameter<std::string>(BuildDirectory,"build",linevec).Value();
			WatchedPatterns = JSL::Parameter<std::vector<std::string>>(WatchedPatterns,"watch",linevec).Value();
			IgnoredPatterns = JSL::Parameter<std::vector<std::string>>(IgnoredPatterns,"ignore",linevec).Value();
			StructureDelimiterRepeatCount = JSL::Parameter<size_t>(StructureDelimiterRepeatCount,"delimiter-repeat",linevec).Value();
		}
		std::string ToText()
		{
			std::ostringstream s;
			s << "i " << JSL::MakeString(TargetDirectory) << "\n";
			s << "directory " << JSL::MakeString(OutputDirectory) << "\n";
			s << "build " << JSL::MakeString(BuildDirectory) << "\n";
			s << "watch " << JSL::MakeString(WatchedPatterns) << "\n";
			s << "ignore " << JSL::MakeString(IgnoredPatterns) << "\n";
			s << "delimiter-repeat " << JSL::MakeString(StructureDelimiterRepeatCount) << "\n";
			return s.str();
		}
		void Help(JSL::HelpMessages & help)
		{
			help.AddMessage("SettingsObject_Files","i",".","TargetDirectory","The target directory to launch the cortex process in. If no argument is provided, activates in the current working directory.");
			help.AddMessage("SettingsObject_Files","directory","compiled","OutputDirectory","The name of the output directory which will contain the compiled pdf");
			help.AddMessage("SettingsObject_Files","build",".cortex/build","BuildDirectory","The name of the directory into which the autogenerated tex will be placed");
			help.AddMessage("SettingsObject_Files","watch",(std::vector<std::string>){"*.tex","*.dat"},"WatchedPatterns","The file patterns included in the indexing process");
			help.AddMessage("SettingsObject_Files","ignore",(std::vector<std::string>){"*.git*","*.build*","*.cortex","*sentinel*"},"IgnoredPatterns","Directory name patterns which are not watched for new files");
			help.AddMessage("SettingsObject_Files","delimiter-repeat",3,"StructureDelimiterRepeatCount","The number of sequential, identical characters required to indicate that a line is a structural delimiter for note metadat");
		}
		void GetDescription(std::string parameter,std::vector<JSL::ParameterDescription> & found)
		{
			JSL::ParameterDescription("TargetDirectory","std::string","i",TargetDirectory,(std::string)".","The target directory to launch the cortex process in. If no argument is provided, activates in the current working directory.").Query(parameter,found);
			JSL::ParameterDescription("OutputDirectory","std::string","directory",OutputDirectory,(std::string)"compiled","The name of the output directory which will contain the compiled pdf").Query(parameter,found);
			JSL::ParameterDescription("BuildDirectory","std::string","build",BuildDirectory,(std::string)".cortex/build","The name of the directory into which the autogenerated tex will be placed").Query(parameter,found);
			JSL::ParameterDescription("WatchedPatterns","std::vector<std::string>","watch",WatchedPatterns,(std::vector<std::string>){"*.tex","*.dat"},"The file patterns included in the indexing process").Query(parameter,found);
			JSL::ParameterDescription("IgnoredPatterns","std::vector<std::string>","ignore",IgnoredPatterns,(std::vector<std::string>){"*.git*","*.build*","*.cortex","*sentinel*"},"Directory name patterns which are not watched for new files").Query(parameter,found);
			JSL::ParameterDescription("StructureDelimiterRepeatCount","size_t","delimiter-repeat",StructureDelimiterRepeatCount,(size_t)3,"The number of sequential, identical characters required to indicate that a line is a structural delimiter for note metadat").Query(parameter,found);
		}
};
class SettingsObject_Document
{
	public:
		SettingsObject_Document(){}
		SettingsObject_Document(int argc,char** argv){Parse(argc,argv);}

		size_t Width = 10;
		size_t FontSize = 10;
		std::vector<std::string> Packages = {"xcolor","amssymb","amsmath"};
		void Parse(int argc, char** argv)
		{
			Width = JSL::Parameter<size_t>(Width,"width",argc,argv).Value();
			FontSize = JSL::Parameter<size_t>(FontSize,"text-size",argc,argv).Value();
			Packages = JSL::Parameter<std::vector<std::string>>(Packages,"package",argc,argv).Value();
		}
		void Configure(const std::string & configFile, std::string configDelimiter)
		{
			Width = JSL::Parameter<size_t>(Width,"width",configFile,configDelimiter).Value();
			FontSize = JSL::Parameter<size_t>(FontSize,"text-size",configFile,configDelimiter).Value();
			Packages = JSL::Parameter<std::vector<std::string>>(Packages,"package",configFile,configDelimiter).Value();
		}
		void ParseLine(const std::vector<std::string> & linevec)
		{
			Width = JSL::Parameter<size_t>(Width,"width",linevec).Value();
			FontSize = JSL::Parameter<size_t>(FontSize,"text-size",linevec).Value();
			Packages = JSL::Parameter<std::vector<std::string>>(Packages,"package",linevec).Value();
		}
		std::string ToText()
		{
			std::ostringstream s;
			s << "width " << JSL::MakeString(Width) << "\n";
			s << "text-size " << JSL::MakeString(FontSize) << "\n";
			s << "package " << JSL::MakeString(Packages) << "\n";
			return s.str();
		}
		void Help(JSL::HelpMessages & help)
		{
			help.AddMessage("SettingsObject_Document","width",10,"Width","The width of each standalone document (measured in cm)");
			help.AddMessage("SettingsObject_Document","text-size",10,"FontSize","The font size (in pt) of the body text of the documents");
			help.AddMessage("SettingsObject_Document","package",(std::vector<std::string>){"xcolor","amssymb","amsmath"},"Packages","Latex packages which are included in the global shared preamble");
		}
		void GetDescription(std::string parameter,std::vector<JSL::ParameterDescription> & found)
		{
			JSL::ParameterDescription("Width","size_t","width",Width,(size_t)10,"The width of each standalone document (measured in cm)").Query(parameter,found);
			JSL::ParameterDescription("FontSize","size_t","text-size",FontSize,(size_t)10,"The font size (in pt) of the body text of the documents").Query(parameter,found);
			JSL::ParameterDescription("Packages","std::vector<std::string>","package",Packages,(std::vector<std::string>){"xcolor","amssymb","amsmath"},"Latex packages which are included in the global shared preamble").Query(parameter,found);
		}
};
class SettingsObject
{
	public:
		SettingsObject(){}
		SettingsObject(int argc,char** argv){Parse(argc,argv);}

		SettingsObject_System System;
		SettingsObject_Files Files;
		SettingsObject_Document Document;
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
			Document.Parse(argc,argv);
		}
		void Configure(const std::string & configFile, std::string configDelimiter)
		{
			System.Configure(configFile,configDelimiter);
			Files.Configure(configFile,configDelimiter);
			Document.Configure(configFile,configDelimiter);
		}
		void ParseLine(const std::vector<std::string> & linevec)
		{
			System.ParseLine(linevec);
			Files.ParseLine(linevec);
			Document.ParseLine(linevec);
		}
		std::string ToText()
		{
			std::ostringstream s;
			s << System.ToText();
			s << Files.ToText();
			s << Document.ToText();
			return s.str();
		}
		void Help(JSL::HelpMessages & help)
		{
			System.Help(help);
			Files.Help(help);
			Document.Help(help);
		}
		std::vector<JSL::ParameterDescription>  GetDescription(std::string parameter)
		{
			auto firstLetter = parameter.find_first_not_of('-');
			if (firstLetter != std::string::npos){
				parameter.erase(0, firstLetter);
			}
			else
			{
				parameter.clear();
			}
			std::vector<JSL::ParameterDescription> found;
			
			System.GetDescription(parameter,found);
			Files.GetDescription(parameter,found);
			Document.GetDescription(parameter,found);
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