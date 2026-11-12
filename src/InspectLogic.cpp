#include "InspectLogic.h"

#include <algorithm>
#include <tuple>

namespace dvb::InspectLogic
{
	std::optional<LightsScope> ParseLightsScope(std::string_view a_scope)
	{
		if (a_scope.empty() || a_scope == "ref")
			return LightsScope::kRef;
		if (a_scope == "scene")
			return LightsScope::kScene;
		return std::nullopt;
	}

	std::vector<std::string> TraversalBudget::TruncatedBy() const
	{
		std::vector<std::string> out;
		if (m_nodesExhausted)
			out.emplace_back("nodes");
		if (m_parentsExhausted)
			out.emplace_back("parentSteps");
		if (m_outputExhausted)
			out.emplace_back("output");
		return out;
	}

	void OrderScene(std::vector<SceneEntry>& a_entries)
	{
		std::stable_sort(a_entries.begin(), a_entries.end(), [](const SceneEntry& a, const SceneEntry& b) {
			if (a.distance.has_value() != b.distance.has_value())
				return a.distance.has_value();
			if (a.distance && *a.distance != *b.distance)
				return *a.distance < *b.distance;
			return std::tie(a.name, a.path, a.x, a.y, a.z) < std::tie(b.name, b.path, b.x, b.y, b.z);
		});
	}

	bool ArtAttachedOrNoSpell(const HandObservation& a_hand)
	{
		return !a_hand.holdsSpell || (a_hand.casterPresent && a_hand.artAttached);
	}

	namespace
	{
		bool HandReady(const HandObservation& a_hand)
		{
			if (!a_hand.holdsSpell)
				return true;
			return a_hand.casterPresent && a_hand.artAttached && !a_hand.artLoading && !a_hand.artPending && a_hand.artNodePresent;
		}
	}

	bool HandsReady(const HandsObservation& a_hands)
	{
		return a_hands.settledDrawn && HandReady(a_hands.left) && HandReady(a_hands.right);
	}

	std::string BackgroundWarning(bool a_focused, std::optional<bool> a_alwaysActive)
	{
		if (a_focused || !a_alwaysActive || *a_alwaysActive)
			return {};
		return "the game window is in the background and bAlwaysActive is off, so the engine pauses; "
			   "run game setAlwaysActive or set [General] bAlwaysActive=1";
	}

	std::string LowerAscii(std::string_view a_text)
	{
		std::string out(a_text);
		for (auto& c : out)
			if (c >= 'A' && c <= 'Z')
				c = static_cast<char>(c - 'A' + 'a');
		return out;
	}

	bool MatchesAnyFilter(std::string_view a_needle, std::initializer_list<std::string_view> a_candidates)
	{
		if (a_needle.empty())
			return true;
		// Folds each byte as it compares, so no per-candidate string is built (this runs once per reference
		// on the main thread) and the needle's case does not matter either.
		const auto fold = [](char c) { return (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c; };
		for (const auto candidate : a_candidates)
			if (std::search(candidate.begin(), candidate.end(), a_needle.begin(), a_needle.end(), [&](char a, char b) { return fold(a) == fold(b); }) != candidate.end())
				return true;
		return false;
	}
}
