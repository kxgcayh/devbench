#pragma once

#include "ToolRegistry.h"

namespace dvb::Dialogue
{
	ToolDescriptor BuildDescriptor();
	json           Handle(const json& a_args, const ToolContext& a_ctx);
}
