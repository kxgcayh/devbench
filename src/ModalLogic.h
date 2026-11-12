#pragma once

#include <algorithm>
#include <array>
#include <cstdint>
#include <format>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

// Which open menus stop an agent from acting, and what to tell it to do about each. It runs on every tool
// reply and must not touch the main thread, which a pausing menu freezes, so it never reads the UI: the
// menu-open sink snapshots each menu's engine flags once, on open, and passes them in as an OpenMenu.
namespace dvb::Modal
{
	struct Blocker
	{
		std::string name;
		std::string hint;
	};

	inline constexpr std::string_view kMessageBox = "MessageBoxMenu";

	/// The RE::UI_MENU_FLAGS bits that decide blocking, mirrored so this header stays free of the game
	/// headers (GameEvents.cpp static_asserts each against the engine enum).
	inline constexpr std::uint32_t kFlagPausesGame = 1u << 0;
	inline constexpr std::uint32_t kFlagAlwaysOpen = 1u << 1;
	inline constexpr std::uint32_t kFlagUsesCursor = 1u << 2;
	inline constexpr std::uint32_t kFlagModal = 1u << 4;
	inline constexpr std::uint32_t kFlagFreezeFramePause = 1u << 24;

	/// An open menu and, when the sink could read the menu object, its engine flags.
	struct OpenMenu
	{
		std::string                  name;
		std::optional<std::uint32_t> flags;
	};

	/// Menus that are open during normal play and never block.
	inline bool IsBenign(std::string_view a_name)
	{
		static constexpr std::array<std::string_view, 7> kBenign = {
			"HUD Menu", "Cursor Menu", "Fader Menu", "Mist Menu", "LootMenu", "Console", "Overlay Interaction Menu"
		};
		return std::find(kBenign.begin(), kBenign.end(), a_name) != kBenign.end();
	}

	/// Menus that block whatever their flags say: the hints below are written for them, and a loading
	/// screen or the main menu is blocked state even if a runtime reports odd flags for it.
	inline bool AlwaysBlocks(std::string_view a_name)
	{
		return a_name == kMessageBox || a_name == "Loading Menu" || a_name == "MainMenu" || a_name == "Main Menu";
	}

	/// True if a menu stops the agent acting. Known flags decide (the same rule as the blocked-menu check in
	/// Tools.cpp): an always-open menu never blocks, otherwise a menu that pauses the game, is modal, takes
	/// the cursor or freeze-frames does. A mod's HUD overlay (TrueHUD, an oxygen meter, a cast bar) sets none
	/// of those. Without flags (the menu object was not readable when it opened) only the name can say, and
	/// an unknown name counts as blocking so a real modal is never hidden.
	inline bool IsBlocking(std::string_view a_name, std::optional<std::uint32_t> a_flags)
	{
		if (IsBenign(a_name))
			return false;
		if (AlwaysBlocks(a_name) || !a_flags)
			return true;
		const std::uint32_t f = *a_flags;
		if (f & kFlagAlwaysOpen)
			return false;
		return (f & (kFlagPausesGame | kFlagModal | kFlagUsesCursor | kFlagFreezeFramePause)) != 0;
	}

	/// What the agent should do about a blocking menu.
	inline std::string HintFor(std::string_view a_name)
	{
		if (a_name == kMessageBox)
			return "menu action=describe to read it, then menu action=accept index=N (close does not dismiss a message box)";
		if (a_name == "Loading Menu")
			return "a load or door transition is in progress; poll menu action=list until it closes";
		if (a_name == "MainMenu" || a_name == "Main Menu")
			return "the game is at the main menu; load a save with game action=load or loadLast";
		return std::format("menu action=close name=\"{}\"", a_name);
	}

	/// The open menus that block, sorted by name, each with its hint.
	inline std::vector<Blocker> Blockers(const std::vector<std::string>& a_openMenus)
	{
		std::vector<Blocker> out;
		for (const auto& name : a_openMenus)
			if (!IsBenign(name))
				out.push_back({ name, HintFor(name) });
		std::sort(out.begin(), out.end(), [](const Blocker& a, const Blocker& b) { return a.name < b.name; });
		return out;
	}

	/// Same as Blockers, for menus whose flags are known. A different name, not an overload: a braced
	/// list of string literals would also match vector<OpenMenu>'s iterator-pair constructor.
	inline std::vector<Blocker> BlockersOf(const std::vector<OpenMenu>& a_open)
	{
		std::vector<Blocker> out;
		for (const auto& m : a_open)
			if (IsBlocking(m.name, m.flags))
				out.push_back({ m.name, HintFor(m.name) });
		std::sort(out.begin(), out.end(), [](const Blocker& a, const Blocker& b) { return a.name < b.name; });
		return out;
	}

	/// The open menus that do not block (HUD, overlays, always-open menus), sorted by name.
	inline std::vector<std::string> HudMenus(const std::vector<OpenMenu>& a_open)
	{
		std::vector<std::string> out;
		for (const auto& m : a_open)
			if (!IsBlocking(m.name, m.flags))
				out.push_back(m.name);
		std::sort(out.begin(), out.end());
		return out;
	}

	/// True if a_name may be auto-dismissed with a plain hide: a message box needs an answered button instead.
	inline bool CanAutoDismiss(std::string_view a_name)
	{
		return !a_name.empty() && a_name != kMessageBox && !IsBenign(a_name);
	}
}
