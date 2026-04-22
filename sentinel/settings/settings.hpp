/*
	This header file was auto-generated from a .hson template
	WARNING: Do not make manual modifications, as they may be overwritten.
*/
#pragma once
#include "JSL/modules/Parameters/Describer.h"
#include "JSL/modules/Parameters/Parsing.h"
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
		int RecursionLimit = 10;
		int RecursionDelay = 100;

		//default spaceship operator
		auto operator<=>(const SettingsObject_System_Headless&) const = default;

		//Comprehension functions
		void Parse(int argc, char** argv);
		void Configure(const std::string & configFile, std::string configDelimiter);
//		Used for single-pass parsings during runtime reconfig
		void ParseLine(const std::vector<std::string> & linevec);
		std::string ToText();
		void Help(JSL::HelpMessages & help);
		void GetDescription(std::string parameter,std::vector<JSL::ParameterDescription> & found);
};
class SettingsObject_System
{
	public:
		SettingsObject_System(){}
		SettingsObject_System(int argc,char** argv){Parse(argc,argv);}

		int DispatchDelay = 10;
		bool Verbose = false;
		bool VeryVerbose = false;
		size_t VeryVerboseWidth = 60;
		bool Quiet = false;
		std::string TerminationFileName = "cortex_disable_message";
		size_t PollingDelay = 100;
		bool Pause = false;
		SettingsObject_System_Headless Headless;

		//default spaceship operator
		auto operator<=>(const SettingsObject_System&) const = default;

		//Comprehension functions
		void Parse(int argc, char** argv);
		void Configure(const std::string & configFile, std::string configDelimiter);
//		Used for single-pass parsings during runtime reconfig
		void ParseLine(const std::vector<std::string> & linevec);
		std::string ToText();
		void Help(JSL::HelpMessages & help);
		void GetDescription(std::string parameter,std::vector<JSL::ParameterDescription> & found);
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

		//default spaceship operator
		auto operator<=>(const SettingsObject_Files&) const = default;

		//Comprehension functions
		void Parse(int argc, char** argv);
		void Configure(const std::string & configFile, std::string configDelimiter);
//		Used for single-pass parsings during runtime reconfig
		void ParseLine(const std::vector<std::string> & linevec);
		std::string ToText();
		void Help(JSL::HelpMessages & help);
		void GetDescription(std::string parameter,std::vector<JSL::ParameterDescription> & found);
};
class SettingsObject_Compiler
{
	public:
		SettingsObject_Compiler(){}
		SettingsObject_Compiler(int argc,char** argv){Parse(argc,argv);}

		std::vector<std::string> Packages = {"xcolor","amssymb","amsmath","lmodern","hyperref"};
		std::string CompilerCommand = "pdflatex";
		std::vector<std::string> CustomProtectedEnvironments = {};

		//default spaceship operator
		auto operator<=>(const SettingsObject_Compiler&) const = default;

		//Comprehension functions
		void Parse(int argc, char** argv);
		void Configure(const std::string & configFile, std::string configDelimiter);
//		Used for single-pass parsings during runtime reconfig
		void ParseLine(const std::vector<std::string> & linevec);
		std::string ToText();
		void Help(JSL::HelpMessages & help);
		void GetDescription(std::string parameter,std::vector<JSL::ParameterDescription> & found);
};
class SettingsObject_Document
{
	public:
		SettingsObject_Document(){}
		SettingsObject_Document(int argc,char** argv){Parse(argc,argv);}

		size_t Width = 10;
		size_t TitleSize = 18;
		bool TitleCentered = false;
		size_t FontSize = 10;
		bool DefaultEnumerate = false;

		//default spaceship operator
		auto operator<=>(const SettingsObject_Document&) const = default;

		//Comprehension functions
		void Parse(int argc, char** argv);
		void Configure(const std::string & configFile, std::string configDelimiter);
//		Used for single-pass parsings during runtime reconfig
		void ParseLine(const std::vector<std::string> & linevec);
		std::string ToText();
		void Help(JSL::HelpMessages & help);
		void GetDescription(std::string parameter,std::vector<JSL::ParameterDescription> & found);
};
class SettingsObject
{
	public:
		SettingsObject(){}
		SettingsObject(int argc,char** argv){Parse(argc,argv);}

		SettingsObject_System System;
		SettingsObject_Files Files;
		SettingsObject_Compiler Compiler;
		SettingsObject_Document Document;

		//default spaceship operator
		auto operator<=>(const SettingsObject&) const = default;

		//Comprehension functions
		void Parse(int argc, char** argv);
		void Configure(const std::string & configFile, std::string configDelimiter);
//		Used for single-pass parsings during runtime reconfig
		void ParseLine(const std::vector<std::string> & linevec);
		std::string ToText();
		void Help(JSL::HelpMessages & help);
		std::vector<JSL::ParameterDescription> GetDescription(std::string parameter);
		void SaveConfig(std::string file);
	private:
		void MasterHelp();
};