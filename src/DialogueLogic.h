#pragma once

#include "ToolRegistry.h"

namespace dvb::DialogueLogic
{
	std::string StateName(const json& a_state);
	bool        Ready(const json& a_view);
	std::string Snapshot(const json& a_view);
	std::size_t ValidateSelection(const json& a_args, const json& a_view);
}
