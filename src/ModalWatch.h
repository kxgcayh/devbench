#pragma once

#include "EventBus.h"
#include "ToolRegistry.h"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace dvb::ModalWatch
{
	/// Wires the reply annotator (a blocking menu or a posted notice rides on every tool reply),
	/// registers the `notice` tool, and arms the auto-dismiss list.
	void Init(ToolRegistry& a_registry, EventBus& a_events, std::vector<std::string> a_autoDismissMenus);

	/// From the menu open/close sink: publishes modal events and hides an auto-dismissed menu. a_flags are the
	/// opened menu's engine flags (nullopt when closing or unreadable); a menu they say never blocks is ignored.
	void OnMenuEvent(const std::string& a_name, bool a_opening, std::optional<std::uint32_t> a_flags = std::nullopt);

	/// Replaces the auto-dismiss list; a message box is never accepted.
	void                                   SetAutoDismiss(const std::vector<std::string>& a_names);
	[[nodiscard]] std::vector<std::string> AutoDismiss();

	/// `blockedBy` / `notices` for the current state, as merged into every reply (empty object if clear).
	[[nodiscard]] json CurrentFields();

	/// `{ menusOpen, hudMenus, modalOpen }` for inspect state/health: every open menu, the ones that block
	/// nothing (HUD overlays), and whether anything blocks. Read from the tracked set, so it is safe off the
	/// main thread and while the game is paused.
	[[nodiscard]] json MenuState();
}
