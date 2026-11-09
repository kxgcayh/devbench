#include "DialogueLogic.h"

#include <cstdint>

namespace dvb::DialogueLogic
{
	std::string StateName(const json& a_state)
	{
		if (a_state == 0)
			return "greeting";
		if (a_state == 1)
			return "topics";
		if (a_state == 2)
			return "topicClicked";
		if (a_state == 3)
			return "transitioning";
		return "unknown";
	}

	bool Ready(const json& a_view)
	{
		return a_view.value("open", false) && a_view.value("supported", false) &&
		       a_view.at("menuState") == 1 && a_view.at("allowProgress") == true &&
		       a_view.at("fadedIn") == true && a_view.at("listVisible") == true &&
		       a_view.at("inputDisabled") == false && a_view.at("selectionDisabled") == false && !a_view.at("topics").empty();
	}

	std::string Snapshot(const json& a_view)
	{
		// This is an observed-content check, not a conversation/session identifier.
		const json    content = { { "speaker", a_view.at("speaker") }, { "topics", a_view.at("topics") } };
		const auto    bytes = content.dump(-1, ' ', false, json::error_handler_t::replace);
		std::uint64_t hash = 14695981039346656037ULL;
		for (unsigned char byte : bytes) {
			hash ^= byte;
			hash *= 1099511628211ULL;
		}
		return std::to_string(hash);
	}

	std::size_t ValidateSelection(const json& a_args, const json& a_view)
	{
		if (!a_args.contains("snapshot") || !a_args["snapshot"].is_string() || a_args["snapshot"].get_ref<const std::string&>().empty())
			throw ToolError(400, "select requires the snapshot from dialogue inspect");
		if (!a_args.contains("index") || !a_args["index"].is_number_integer() || a_args["index"] < 0 || a_args["index"] > 511)
			throw ToolError(400, "index must be an integer in [0,511]");
		if (!a_view.value("open", false))
			throw ToolError(409, "no active Dialogue Menu");
		if (!a_view.value("supported", false))
			throw ToolError(422, "Dialogue Menu does not expose the supported topic interface");
		if (!Ready(a_view))
			throw ToolError(409, "dialogue is not ready for a topic selection; inspect progress before retrying");
		if (a_args["snapshot"] != Snapshot(a_view))
			throw ToolError(409, "speaker or topic list changed; inspect dialogue again");
		const auto index = a_args["index"].get<std::size_t>();
		if (index >= a_view.at("topics").size())
			throw ToolError(400, "index is outside the current topic list");
		return index;
	}
}
