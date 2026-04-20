#include "settings.hpp"

#include "JSL/modules/Parameters/Parameter.h"
#include "JSL/modules/FileIO/fileWriters.h"
#include <cstdlib>//for exit(0)
void SettingsObject_System_Headless::Parse(int argc, char** argv)
{
	Active = JSL::Parameter<bool>(Active,"headless",argc,argv).Value();
	CortexName = JSL::Parameter<std::string>(CortexName,"headless-name",argc,argv).Value();
	AuthorName = JSL::Parameter<std::string>(AuthorName,"headless-author",argc,argv).Value();
	RecursionLimit = JSL::Parameter<int>(RecursionLimit,"headless-recursion",argc,argv).Value();
	RecursionDelay = JSL::Parameter<int>(RecursionDelay,"headless-delay",argc,argv).Value();
}
void SettingsObject_System_Headless::Configure(const std::string & configFile, std::string configDelimiter)
{
	Active = JSL::Parameter<bool>(Active,"headless",configFile,configDelimiter).Value();
	CortexName = JSL::Parameter<std::string>(CortexName,"headless-name",configFile,configDelimiter).Value();
	AuthorName = JSL::Parameter<std::string>(AuthorName,"headless-author",configFile,configDelimiter).Value();
	RecursionLimit = JSL::Parameter<int>(RecursionLimit,"headless-recursion",configFile,configDelimiter).Value();
	RecursionDelay = JSL::Parameter<int>(RecursionDelay,"headless-delay",configFile,configDelimiter).Value();
}
void SettingsObject_System_Headless::ParseLine(const std::vector<std::string> & linevec)
{
	Active = JSL::Parameter<bool>(Active,"headless",linevec).Value();
	CortexName = JSL::Parameter<std::string>(CortexName,"headless-name",linevec).Value();
	AuthorName = JSL::Parameter<std::string>(AuthorName,"headless-author",linevec).Value();
	RecursionLimit = JSL::Parameter<int>(RecursionLimit,"headless-recursion",linevec).Value();
	RecursionDelay = JSL::Parameter<int>(RecursionDelay,"headless-delay",linevec).Value();
}
std::string SettingsObject_System_Headless::ToText()
{
	std::ostringstream s;
	s << "headless " << JSL::MakeString(Active) << "\n";
	s << "headless-name " << JSL::MakeString(CortexName) << "\n";
	s << "headless-author " << JSL::MakeString(AuthorName) << "\n";
	s << "headless-recursion " << JSL::MakeString(RecursionLimit) << "\n";
	s << "headless-delay " << JSL::MakeString(RecursionDelay) << "\n";
	return s.str();}
void SettingsObject_System_Headless::Help(JSL::HelpMessages & help)
{
	help.AddMessage("SettingsObject_System_Headless","headless",false,"Active","If true, runs the system in headless mode");
	help.AddMessage("SettingsObject_System_Headless","headless-name","My Project","CortexName","The default name given to a new cortex instantiation if not provided by the user");
	help.AddMessage("SettingsObject_System_Headless","headless-author","Me","AuthorName","The default author assigned to a new cortex instantiation if not provided by the user");
	help.AddMessage("SettingsObject_System_Headless","headless-recursion",10,"RecursionLimit","The number of times a signal will be sent before determining the process is dead and unresponsive");
	help.AddMessage("SettingsObject_System_Headless","headless-delay",100,"RecursionDelay","The time (in ms) between each recursive signal to a headless service");
}
void SettingsObject_System_Headless::GetDescription(std::string parameter,std::vector<JSL::ParameterDescription> & found)
{
	JSL::ParameterDescription("Active","bool","headless",Active,(bool)false,"If true, runs the system in headless mode").Query(parameter,found);
	JSL::ParameterDescription("CortexName","std::string","headless-name",CortexName,(std::string)"My Project","The default name given to a new cortex instantiation if not provided by the user").Query(parameter,found);
	JSL::ParameterDescription("AuthorName","std::string","headless-author",AuthorName,(std::string)"Me","The default author assigned to a new cortex instantiation if not provided by the user").Query(parameter,found);
	JSL::ParameterDescription("RecursionLimit","int","headless-recursion",RecursionLimit,(int)10,"The number of times a signal will be sent before determining the process is dead and unresponsive").Query(parameter,found);
	JSL::ParameterDescription("RecursionDelay","int","headless-delay",RecursionDelay,(int)100,"The time (in ms) between each recursive signal to a headless service").Query(parameter,found);
 }
void SettingsObject_System::Parse(int argc, char** argv)
{
	DispatchDelay = JSL::Parameter<int>(DispatchDelay,"delay",argc,argv).Value();
	Verbose = JSL::Parameter<bool>(Verbose,"v",argc,argv).Value();
	VeryVerbose = JSL::Parameter<bool>(VeryVerbose,"vv",argc,argv).Value();
	VeryVerboseWidth = JSL::Parameter<size_t>(VeryVerboseWidth,"v-width",argc,argv).Value();
	Quiet = JSL::Parameter<bool>(Quiet,"q",argc,argv).Value();
	TerminationFileName = JSL::Parameter<std::string>(TerminationFileName,"terminate",argc,argv).Value();
	PollingDelay = JSL::Parameter<size_t>(PollingDelay,"poll-delay",argc,argv).Value();
	Pause = JSL::Parameter<bool>(Pause,"pause",argc,argv).Value();
	Headless.Parse(argc,argv);
}
void SettingsObject_System::Configure(const std::string & configFile, std::string configDelimiter)
{
	DispatchDelay = JSL::Parameter<int>(DispatchDelay,"delay",configFile,configDelimiter).Value();
	Verbose = JSL::Parameter<bool>(Verbose,"v",configFile,configDelimiter).Value();
	VeryVerbose = JSL::Parameter<bool>(VeryVerbose,"vv",configFile,configDelimiter).Value();
	VeryVerboseWidth = JSL::Parameter<size_t>(VeryVerboseWidth,"v-width",configFile,configDelimiter).Value();
	Quiet = JSL::Parameter<bool>(Quiet,"q",configFile,configDelimiter).Value();
	TerminationFileName = JSL::Parameter<std::string>(TerminationFileName,"terminate",configFile,configDelimiter).Value();
	PollingDelay = JSL::Parameter<size_t>(PollingDelay,"poll-delay",configFile,configDelimiter).Value();
	Pause = JSL::Parameter<bool>(Pause,"pause",configFile,configDelimiter).Value();
	Headless.Configure(configFile,configDelimiter);
}
void SettingsObject_System::ParseLine(const std::vector<std::string> & linevec)
{
	DispatchDelay = JSL::Parameter<int>(DispatchDelay,"delay",linevec).Value();
	Verbose = JSL::Parameter<bool>(Verbose,"v",linevec).Value();
	VeryVerbose = JSL::Parameter<bool>(VeryVerbose,"vv",linevec).Value();
	VeryVerboseWidth = JSL::Parameter<size_t>(VeryVerboseWidth,"v-width",linevec).Value();
	Quiet = JSL::Parameter<bool>(Quiet,"q",linevec).Value();
	TerminationFileName = JSL::Parameter<std::string>(TerminationFileName,"terminate",linevec).Value();
	PollingDelay = JSL::Parameter<size_t>(PollingDelay,"poll-delay",linevec).Value();
	Pause = JSL::Parameter<bool>(Pause,"pause",linevec).Value();
	Headless.ParseLine(linevec);
}
std::string SettingsObject_System::ToText()
{
	std::ostringstream s;
	s << "delay " << JSL::MakeString(DispatchDelay) << "\n";
	s << "v " << JSL::MakeString(Verbose) << "\n";
	s << "vv " << JSL::MakeString(VeryVerbose) << "\n";
	s << "v-width " << JSL::MakeString(VeryVerboseWidth) << "\n";
	s << "q " << JSL::MakeString(Quiet) << "\n";
	s << "terminate " << JSL::MakeString(TerminationFileName) << "\n";
	s << "poll-delay " << JSL::MakeString(PollingDelay) << "\n";
	s << "pause " << JSL::MakeString(Pause) << "\n";
	s << Headless.ToText();
	return s.str();}
void SettingsObject_System::Help(JSL::HelpMessages & help)
{
	help.AddMessage("SettingsObject_System","delay",10,"DispatchDelay","The delay time (in ms) between detecting a filechange and dispatching the calls to the manager.");
	help.AddMessage("SettingsObject_System","v",false,"Verbose","Inlcudes debugging error messages. Overrides quiet");
	help.AddMessage("SettingsObject_System","vv",false,"VeryVerbose","Adds additional context to debugging error messages.");
	help.AddMessage("SettingsObject_System","v-width",60,"VeryVerboseWidth","The width (in characters) of the debugging visualisation window");
	help.AddMessage("SettingsObject_System","q",false,"Quiet","Suppresses all outputs except errors.");
	help.AddMessage("SettingsObject_System","terminate","cortex_disable_message","TerminationFileName","If a file with this name appears in a watched directory, cortex will take this as a signal to exit. The file is deleted.");
	help.AddMessage("SettingsObject_System","poll-delay",100,"PollingDelay","The responsiveness delay (in ms) in the menu polling interface");
	help.AddMessage("SettingsObject_System","pause",false,"Pause","If true, disables calls to the compiler");
	Headless.Help(help);
}
void SettingsObject_System::GetDescription(std::string parameter,std::vector<JSL::ParameterDescription> & found)
{
	JSL::ParameterDescription("DispatchDelay","int","delay",DispatchDelay,(int)10,"The delay time (in ms) between detecting a filechange and dispatching the calls to the manager.").Query(parameter,found);
	JSL::ParameterDescription("Verbose","bool","v",Verbose,(bool)false,"Inlcudes debugging error messages. Overrides quiet").Query(parameter,found);
	JSL::ParameterDescription("VeryVerbose","bool","vv",VeryVerbose,(bool)false,"Adds additional context to debugging error messages.").Query(parameter,found);
	JSL::ParameterDescription("VeryVerboseWidth","size_t","v-width",VeryVerboseWidth,(size_t)60,"The width (in characters) of the debugging visualisation window").Query(parameter,found);
	JSL::ParameterDescription("Quiet","bool","q",Quiet,(bool)false,"Suppresses all outputs except errors.").Query(parameter,found);
	JSL::ParameterDescription("TerminationFileName","std::string","terminate",TerminationFileName,(std::string)"cortex_disable_message","If a file with this name appears in a watched directory, cortex will take this as a signal to exit. The file is deleted.").Query(parameter,found);
	JSL::ParameterDescription("PollingDelay","size_t","poll-delay",PollingDelay,(size_t)100,"The responsiveness delay (in ms) in the menu polling interface").Query(parameter,found);
	JSL::ParameterDescription("Pause","bool","pause",Pause,(bool)false,"If true, disables calls to the compiler").Query(parameter,found);
	Headless.GetDescription(parameter,found);
 }
void SettingsObject_Files::Parse(int argc, char** argv)
{
	TargetDirectory = JSL::Parameter<std::string>(TargetDirectory,"i",argc,argv).Value();
	OutputDirectory = JSL::Parameter<std::string>(OutputDirectory,"directory",argc,argv).Value();
	BuildDirectory = JSL::Parameter<std::string>(BuildDirectory,"build",argc,argv).Value();
	WatchedPatterns = JSL::Parameter<std::vector<std::string>>(WatchedPatterns,"watch",argc,argv).Value();
	IgnoredPatterns = JSL::Parameter<std::vector<std::string>>(IgnoredPatterns,"ignore",argc,argv).Value();
	StructureDelimiterRepeatCount = JSL::Parameter<size_t>(StructureDelimiterRepeatCount,"delimiter-repeat",argc,argv).Value();
}
void SettingsObject_Files::Configure(const std::string & configFile, std::string configDelimiter)
{
	TargetDirectory = JSL::Parameter<std::string>(TargetDirectory,"i",configFile,configDelimiter).Value();
	OutputDirectory = JSL::Parameter<std::string>(OutputDirectory,"directory",configFile,configDelimiter).Value();
	BuildDirectory = JSL::Parameter<std::string>(BuildDirectory,"build",configFile,configDelimiter).Value();
	WatchedPatterns = JSL::Parameter<std::vector<std::string>>(WatchedPatterns,"watch",configFile,configDelimiter).Value();
	IgnoredPatterns = JSL::Parameter<std::vector<std::string>>(IgnoredPatterns,"ignore",configFile,configDelimiter).Value();
	StructureDelimiterRepeatCount = JSL::Parameter<size_t>(StructureDelimiterRepeatCount,"delimiter-repeat",configFile,configDelimiter).Value();
}
void SettingsObject_Files::ParseLine(const std::vector<std::string> & linevec)
{
	TargetDirectory = JSL::Parameter<std::string>(TargetDirectory,"i",linevec).Value();
	OutputDirectory = JSL::Parameter<std::string>(OutputDirectory,"directory",linevec).Value();
	BuildDirectory = JSL::Parameter<std::string>(BuildDirectory,"build",linevec).Value();
	WatchedPatterns = JSL::Parameter<std::vector<std::string>>(WatchedPatterns,"watch",linevec).Value();
	IgnoredPatterns = JSL::Parameter<std::vector<std::string>>(IgnoredPatterns,"ignore",linevec).Value();
	StructureDelimiterRepeatCount = JSL::Parameter<size_t>(StructureDelimiterRepeatCount,"delimiter-repeat",linevec).Value();
}
std::string SettingsObject_Files::ToText()
{
	std::ostringstream s;
	s << "i " << JSL::MakeString(TargetDirectory) << "\n";
	s << "directory " << JSL::MakeString(OutputDirectory) << "\n";
	s << "build " << JSL::MakeString(BuildDirectory) << "\n";
	s << "watch " << JSL::MakeString(WatchedPatterns) << "\n";
	s << "ignore " << JSL::MakeString(IgnoredPatterns) << "\n";
	s << "delimiter-repeat " << JSL::MakeString(StructureDelimiterRepeatCount) << "\n";
	return s.str();}
void SettingsObject_Files::Help(JSL::HelpMessages & help)
{
	help.AddMessage("SettingsObject_Files","i",".","TargetDirectory","The target directory to launch the cortex process in. If no argument is provided, activates in the current working directory.");
	help.AddMessage("SettingsObject_Files","directory","compiled","OutputDirectory","The name of the output directory which will contain the compiled pdf");
	help.AddMessage("SettingsObject_Files","build",".cortex/build","BuildDirectory","The name of the directory into which the autogenerated tex will be placed");
	help.AddMessage("SettingsObject_Files","watch",(std::vector<std::string>){"*.tex","*.dat"},"WatchedPatterns","The file patterns included in the indexing process");
	help.AddMessage("SettingsObject_Files","ignore",(std::vector<std::string>){"*.git*","*.build*","*.cortex","*sentinel*"},"IgnoredPatterns","Directory name patterns which are not watched for new files");
	help.AddMessage("SettingsObject_Files","delimiter-repeat",3,"StructureDelimiterRepeatCount","The number of sequential, identical characters required to indicate that a line is a structural delimiter for note metadat");
}
void SettingsObject_Files::GetDescription(std::string parameter,std::vector<JSL::ParameterDescription> & found)
{
	JSL::ParameterDescription("TargetDirectory","std::string","i",TargetDirectory,(std::string)".","The target directory to launch the cortex process in. If no argument is provided, activates in the current working directory.").Query(parameter,found);
	JSL::ParameterDescription("OutputDirectory","std::string","directory",OutputDirectory,(std::string)"compiled","The name of the output directory which will contain the compiled pdf").Query(parameter,found);
	JSL::ParameterDescription("BuildDirectory","std::string","build",BuildDirectory,(std::string)".cortex/build","The name of the directory into which the autogenerated tex will be placed").Query(parameter,found);
	JSL::ParameterDescription("WatchedPatterns","std::vector<std::string>","watch",WatchedPatterns,(std::vector<std::string>){"*.tex","*.dat"},"The file patterns included in the indexing process").Query(parameter,found);
	JSL::ParameterDescription("IgnoredPatterns","std::vector<std::string>","ignore",IgnoredPatterns,(std::vector<std::string>){"*.git*","*.build*","*.cortex","*sentinel*"},"Directory name patterns which are not watched for new files").Query(parameter,found);
	JSL::ParameterDescription("StructureDelimiterRepeatCount","size_t","delimiter-repeat",StructureDelimiterRepeatCount,(size_t)3,"The number of sequential, identical characters required to indicate that a line is a structural delimiter for note metadat").Query(parameter,found);
 }
void SettingsObject_Compiler::Parse(int argc, char** argv)
{
	Packages = JSL::Parameter<std::vector<std::string>>(Packages,"package",argc,argv).Value();
	CompilerCommand = JSL::Parameter<std::string>(CompilerCommand,"compiler",argc,argv).Value();
}
void SettingsObject_Compiler::Configure(const std::string & configFile, std::string configDelimiter)
{
	Packages = JSL::Parameter<std::vector<std::string>>(Packages,"package",configFile,configDelimiter).Value();
	CompilerCommand = JSL::Parameter<std::string>(CompilerCommand,"compiler",configFile,configDelimiter).Value();
}
void SettingsObject_Compiler::ParseLine(const std::vector<std::string> & linevec)
{
	Packages = JSL::Parameter<std::vector<std::string>>(Packages,"package",linevec).Value();
	CompilerCommand = JSL::Parameter<std::string>(CompilerCommand,"compiler",linevec).Value();
}
std::string SettingsObject_Compiler::ToText()
{
	std::ostringstream s;
	s << "package " << JSL::MakeString(Packages) << "\n";
	s << "compiler " << JSL::MakeString(CompilerCommand) << "\n";
	return s.str();}
void SettingsObject_Compiler::Help(JSL::HelpMessages & help)
{
	help.AddMessage("SettingsObject_Compiler","package",(std::vector<std::string>){"xcolor","amssymb","amsmath","lmodern","hyperref"},"Packages","Latex packages which are included in the global shared preamble");
	help.AddMessage("SettingsObject_Compiler","compiler","pdflatex","CompilerCommand","The latex compiler used to generate files. Valid choices are 'pdflatex', 'xelatex', and 'lualatex'");
}
void SettingsObject_Compiler::GetDescription(std::string parameter,std::vector<JSL::ParameterDescription> & found)
{
	JSL::ParameterDescription("Packages","std::vector<std::string>","package",Packages,(std::vector<std::string>){"xcolor","amssymb","amsmath","lmodern","hyperref"},"Latex packages which are included in the global shared preamble").Query(parameter,found);
	JSL::ParameterDescription("CompilerCommand","std::string","compiler",CompilerCommand,(std::string)"pdflatex","The latex compiler used to generate files. Valid choices are 'pdflatex', 'xelatex', and 'lualatex'").Query(parameter,found);
 }
void SettingsObject_Document::Parse(int argc, char** argv)
{
	Width = JSL::Parameter<size_t>(Width,"width",argc,argv).Value();
	TitleSize = JSL::Parameter<size_t>(TitleSize,"title-size",argc,argv).Value();
	TitleCentered = JSL::Parameter<bool>(TitleCentered,"title-center",argc,argv).Value();
	FontSize = JSL::Parameter<size_t>(FontSize,"text-size",argc,argv).Value();
}
void SettingsObject_Document::Configure(const std::string & configFile, std::string configDelimiter)
{
	Width = JSL::Parameter<size_t>(Width,"width",configFile,configDelimiter).Value();
	TitleSize = JSL::Parameter<size_t>(TitleSize,"title-size",configFile,configDelimiter).Value();
	TitleCentered = JSL::Parameter<bool>(TitleCentered,"title-center",configFile,configDelimiter).Value();
	FontSize = JSL::Parameter<size_t>(FontSize,"text-size",configFile,configDelimiter).Value();
}
void SettingsObject_Document::ParseLine(const std::vector<std::string> & linevec)
{
	Width = JSL::Parameter<size_t>(Width,"width",linevec).Value();
	TitleSize = JSL::Parameter<size_t>(TitleSize,"title-size",linevec).Value();
	TitleCentered = JSL::Parameter<bool>(TitleCentered,"title-center",linevec).Value();
	FontSize = JSL::Parameter<size_t>(FontSize,"text-size",linevec).Value();
}
std::string SettingsObject_Document::ToText()
{
	std::ostringstream s;
	s << "width " << JSL::MakeString(Width) << "\n";
	s << "title-size " << JSL::MakeString(TitleSize) << "\n";
	s << "title-center " << JSL::MakeString(TitleCentered) << "\n";
	s << "text-size " << JSL::MakeString(FontSize) << "\n";
	return s.str();}
void SettingsObject_Document::Help(JSL::HelpMessages & help)
{
	help.AddMessage("SettingsObject_Document","width",10,"Width","The width of each standalone document (measured in cm)");
	help.AddMessage("SettingsObject_Document","title-size",18,"TitleSize","The font size (in pt) of the title text of the documents");
	help.AddMessage("SettingsObject_Document","title-center",false,"TitleCentered","If true, the title of all notes are centered on the page");
	help.AddMessage("SettingsObject_Document","text-size",10,"FontSize","The font size (in pt) of the body text of the documents");
}
void SettingsObject_Document::GetDescription(std::string parameter,std::vector<JSL::ParameterDescription> & found)
{
	JSL::ParameterDescription("Width","size_t","width",Width,(size_t)10,"The width of each standalone document (measured in cm)").Query(parameter,found);
	JSL::ParameterDescription("TitleSize","size_t","title-size",TitleSize,(size_t)18,"The font size (in pt) of the title text of the documents").Query(parameter,found);
	JSL::ParameterDescription("TitleCentered","bool","title-center",TitleCentered,(bool)false,"If true, the title of all notes are centered on the page").Query(parameter,found);
	JSL::ParameterDescription("FontSize","size_t","text-size",FontSize,(size_t)10,"The font size (in pt) of the body text of the documents").Query(parameter,found);
 }
void SettingsObject::Parse(int argc, char** argv)
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
	Compiler.Parse(argc,argv);
	Document.Parse(argc,argv);
}
void SettingsObject::Configure(const std::string & configFile, std::string configDelimiter)
{
	System.Configure(configFile,configDelimiter);
	Files.Configure(configFile,configDelimiter);
	Compiler.Configure(configFile,configDelimiter);
	Document.Configure(configFile,configDelimiter);
}
void SettingsObject::ParseLine(const std::vector<std::string> & linevec)
{
	System.ParseLine(linevec);
	Files.ParseLine(linevec);
	Compiler.ParseLine(linevec);
	Document.ParseLine(linevec);
}
std::string SettingsObject::ToText()
{
	std::ostringstream s;
	s << System.ToText();
	s << Files.ToText();
	s << Compiler.ToText();
	s << Document.ToText();
	return s.str();}
void SettingsObject::Help(JSL::HelpMessages & help)
{
	System.Help(help);
	Files.Help(help);
	Compiler.Help(help);
	Document.Help(help);
}
std::vector<JSL::ParameterDescription> SettingsObject::GetDescription(std::string parameter)
{
	auto firstLetter = parameter.find_first_not_of('-');
	if (firstLetter != std::string::npos)
	{
		parameter.erase(0, firstLetter);
	}
	else
	{
		parameter.clear();
	}
	std::vector<JSL::ParameterDescription> found;
	System.GetDescription(parameter,found);
	Files.GetDescription(parameter,found);
	Compiler.GetDescription(parameter,found);
	Document.GetDescription(parameter,found);
	return found;
}
void SettingsObject::SaveConfig(std::string file)
{
	auto contents = ToText();
	JSL::writeStringToFile(file,contents,std::ios::out);
}
void SettingsObject::MasterHelp()
{
	JSL::HelpMessages help;
	help.AddMessage("SettingsObject","config","__none__","ConfigureFile","When not equal to '__none__', the system will attempt to read this file in as a configuration file.\nConfiguration files work the same as command line arguments, each line should contain a single flag and a value\nIMPORTANT: Flags in config files omit the '-'");
	help.AddMessage("SettingsObject","configure-delimiter"," ","ConfigureDelimiter","The string which separates the flag from the values in the config file.\nOnly the first instance of the flag is counted, subsequent occurrences are ignored.");
	help.AddMessage("SettingsObject","h, --help",false,"Help","When true, activates the help page, then exits");
	Help(help);
	help.print();
	exit(0);
}
