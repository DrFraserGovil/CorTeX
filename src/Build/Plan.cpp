#include "Plan.h"
#include "Util/Text.h"
#include <JSL.h>
#include <functional>
#include <set>

namespace
{
	using Kind = Notes::Reference::Kind;

	// A note's PDF, as a path relative to the page's PDF (both laid out like their sources); a dummy root, so a top-level note works too
	std::string PdfRelative(const Notes::Note &target, const Notes::Note &page)
	{
		auto pdf = ("/root" / target.Path).replace_extension(".pdf");
		return pdf.lexically_relative(("/root" / page.Path).parent_path()).generic_string();
	}
} // namespace

std::string Build::Plan::Target(const Notes::Note &from, const std::string &slug) const
{
	auto id = Net.Resolve(slug, from.Path);
	return id.empty() ? id : Net.Follow(id).value_or("");
}

bool Build::Plan::Circular(const std::string &from, const std::string &to) const
{
	return from == to || Component.at(from) == Component.at(to);
}

Build::Plan::Plan(const Network::Network &network, const Settings::Config &config, Report::Log &report) : Net(network), Config(config)
{
	/// FIND CYCLES: STRONGLY CONNECTED COMPONENTS OF THE TRANSCLUSION GRAPH (TARJAN)
	// an edge is circular if it stays within one component (or points at its own note), wherever the compilation starts
	std::map<std::string, int> order, low;
	std::vector<std::string> stack;
	std::set<std::string> onStack;
	int counter = 0, components = 0;
	std::function<void(const Network::Node &)> connect = [&](const Network::Node &node) {
		auto &id = node.Id;
		order[id] = low[id] = counter++;
		stack.push_back(id);
		onStack.insert(id);
		for (auto &reference : node.Source->References)
		{
			if (reference.Type != Kind::Transclusion) continue;
			auto next = Target(*node.Source, reference.Slug);
			if (next.empty()) continue;
			if (!order.contains(next))
			{
				connect(*Net.Find(next));
				low[id] = std::min(low[id], low[next]);
			}
			else if (onStack.contains(next))
			{
				low[id] = std::min(low[id], order[next]);
			}
		}
		if (low[id] == order[id])
		{
			std::string member;
			do
			{
				member = stack.back();
				stack.pop_back();
				onStack.erase(member);
				Component[member] = components;
			} while (member != id);
			++components;
		}
	};
	for (auto &node : Net.Nodes)
	{
		if (!order.contains(node.Id)) connect(node);
	}

	// each circular transclusion is reported once, at the place it was written
	for (auto &node : Net.Nodes)
	{
		auto &note = *node.Source;
		for (auto &reference : note.References)
		{
			if (reference.Type != Kind::Transclusion) continue;
			auto next = Target(note, reference.Slug);
			if (!next.empty() && Circular(note.Id, next))
			{
				report.Add(Report::Kind::Transclusion, "transcluding '" + reference.Target + "' here would loop back on itself, so it is shown as an error instead", note.Path,
						   note.BodyLine + reference.Line - 1);
			}
		}
	}
}

Tex::Job Build::Plan::JobFor(const Notes::Note &page, const BuildState &state) const
{
	Tex::Job job;
	job.Note = &page;
	job.ShowTitle = page.Fields.ShowTitle.value_or(Config.Compilation.InsertTitle);
	bool pdf = Config.Compilation.Pdf;

	/// EVERYTHING REACHABLE THROUGH (NON-CIRCULAR) TRANSCLUSIONS, IN FIRST-VISIT ORDER: THEIR PREAMBLES ARE HOISTED IN THIS ORDER
	std::set<std::string> visited{page.Id};
	std::function<void(const Notes::Note &)> visit = [&](const Notes::Note &from) {
		for (auto &reference : from.References)
		{
			if (reference.Type != Kind::Transclusion) continue;
			auto next = Target(from, reference.Slug);
			if (next.empty() || Circular(from.Id, next) || !visited.insert(next).second) continue;
			auto *note = Net.Find(next)->Source;
			job.Hoisted.push_back(note);
			visit(*note);
		}
	};
	visit(page);
	std::vector<const Notes::Note *> shown{&page};
	shown.insert(shown.end(), job.Hoisted.begin(), job.Hoisted.end());
	std::set<std::string> shownIds;
	for (auto *note : shown) shownIds.insert(note->Id);

	/// THE LABELS ON THE PAGE: EACH NAMED, SO ITS \label CAN LEAVE A MARK TO SCROLL TO
	std::set<std::string> pageLabels;
	for (auto *note : shown)
	{
		for (auto &label : note->Labels)
		{
			if (pageLabels.insert(label.Name).second) job.Declarations.push_back("\\CortexLabelName{" + label.Name + "}{" + Tex::Hex(label.Name) + "}");
		}
	}

	/// DECLARATIONS FOR THE NOTE AND EVERY NOTE IT SHOWS: EACH RESOLVED FROM ITS OWN ORIGIN
	for (auto *from : shown)
	{
		auto *node = Net.Find(from->Id);
		std::set<std::string> declared;
		size_t labelIndex = 0;
		for (auto &reference : from->References)
		{
			if (reference.Type == Kind::Transclusion)
			{
				if (!declared.insert("!" + reference.Slug).second) continue;
				auto next = Target(*from, reference.Slug);
				auto declare = "{" + from->Key + "}{" + reference.Slug + "}{";
				if (next.empty())
				{
					job.Declarations.push_back("\\CortexTranscludeProblem" + declare + "missing note: " + reference.Slug + "}");
				}
				else if (Circular(from->Id, next))
				{
					job.Declarations.push_back("\\CortexTranscludeProblem" + declare + "circular transclusion: " + reference.Slug + "}");
				}
				else
				{
					// the transcluded note's folder, relative to the page's (where TeX runs), is searched first while its text is input
					auto *target = Net.Find(next)->Source;
					auto folder = target->Path.parent_path().lexically_relative(page.Path.parent_path()).generic_string();
					if (folder == ".") folder = "";
					job.Declarations.push_back("\\CortexTranscludeTarget" + declare + Tex::Compiler::FragmentName(*target) + "}{" + folder + "}");
				}
			}
			else if (reference.Type == Kind::Label)
			{
				// node->Labels lists the label references in the order written, resolved
				auto &resolved = node->Labels[labelIndex++];
				if (!declared.insert(reference.Slug + "#" + reference.Label).second) continue;
				auto hex = Tex::Hex(reference.Label);
				bool local = reference.Slug.empty() ? pageLabels.contains(reference.Label) : (!resolved.Owner.empty() && shownIds.contains(resolved.Owner));
				std::string shownText, action;
				if (local)
				{
					shownText = "\\ref{" + reference.Label + "}";
					action = pdf ? "/S/GoTo /D <" + hex + ">" : "l/" + page.Id + "/" + hex;
				}
				else if (!resolved.Owner.empty())
				{
					// the number as the owner last printed it; ?? until it has compiled
					shownText = "??";
					if (auto record = state.Records.find(resolved.Owner); record != state.Records.end())
					{
						if (auto found = record->second.Labels.find(reference.Label); found != record->second.Labels.end()) shownText = found->second;
					}
					auto &owner = *Net.Find(resolved.Owner)->Source;
					action = pdf ? "/S/GoToR /F <" + Tex::Hex(PdfRelative(owner, page)) + "> /D <" + hex + ">" : "l/" + owner.Id + "/" + hex;
				}
				else
				{
					// the label isn't there, but the note named is: the reference still opens it
					auto next = reference.Slug.empty() ? "" : Target(*from, reference.Slug);
					if (next.empty()) continue;
					shownText = "??";
					action = pdf ? "/S/GoToR /F <" + Tex::Hex(PdfRelative(*Net.Find(next)->Source, page)) + "> /D[0 /Fit]" : "a/" + from->Key + "/" + reference.Slug;
				}
				job.Declarations.push_back("\\CortexRefTarget{" + from->Key + "}{" + reference.Slug + "}{" + reference.Label + "}{" + shownText + "}{" + action + "}");
			}
			else if (pdf)
			{
				// a PDF link must name the target's file relative to the PDF actually being read: this page's, not the origin's
				if (!declared.insert(reference.Slug).second) continue;
				auto next = Target(*from, reference.Slug);
				if (next.empty()) continue;
				job.Declarations.push_back("\\CortexLinkTarget{" + from->Key + "}{" + reference.Slug + "}{" + Tex::Hex(PdfRelative(*Net.Find(next)->Source, page)) + "}");
			}
		}
	}
	return job;
}

std::string Build::Plan::Hash(const Tex::Job &job)
{
	// '\0' separates the parts, so that moving text from one part to the next can't produce the same hash
	auto &note = *job.Note;
	std::string key = note.LocalPreamble + '\0' + note.Body + '\0' + (job.ShowTitle ? "title:" + note.Fields.Title : std::string("untitled"));
	for (auto *hoisted : job.Hoisted) key += '\0' + hoisted->Key + '\0' + hoisted->LocalPreamble + '\0' + hoisted->Body;
	for (auto &declaration : job.Declarations) key += '\0' + declaration;
	return Util::HashHex(key);
}
