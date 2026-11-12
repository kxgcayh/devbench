#pragma once

#include "ModalLogic.h"

#include <cstdint>
#include <string>
#include <vector>

namespace dvb
{
	class EventBus;

	/// Names of currently-open menus, tracked live from MenuOpenCloseEvent. Lets the
	/// `menu` tool detect blocking modals (e.g. "MessageBoxMenu") without touching the
	/// UI on the listener thread. Thread-safe.
	std::vector<std::string> GetOpenMenus();

	/// The same set with each menu's engine flags, read once on open (nullopt if the menu object was not
	/// readable then). Thread-safe, never touches the UI. Feeds the blocking classification in ModalLogic.h.
	std::vector<Modal::OpenMenu> GetOpenMenuInfo();

	/// Install game event sources that publish into `a_bus`:
	///  - a MenuOpenCloseEvent sink → "menu" events { name, opening } (loading screens,
	///    main menu, new-game message boxes, etc.).
	/// SKSE-messaging lifecycle events are forwarded separately via OnSKSEMessage.
	/// Call once, after the server's EventBus exists (kDataLoaded).
	void InstallGameEvents(EventBus& a_bus);

	/// Forward an SKSE MessagingInterface message type; publishes a "lifecycle" event
	/// (dataLoaded / newGame / preLoadGame / postLoadGame / saveGame / deleteGame).
	/// No-op until InstallGameEvents has run.
	void OnSKSEMessage(std::uint32_t a_type);
}
