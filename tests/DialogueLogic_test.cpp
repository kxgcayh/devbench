#include "test_framework.h"

#include "DialogueLogic.h"

using namespace dvb;

namespace
{
	json View()
	{
		return { { "open", true }, { "supported", true }, { "speaker", "Innkeeper" },
			{ "menuState", 1 }, { "allowProgress", true }, { "fadedIn", true },
			{ "listVisible", true }, { "inputDisabled", false }, { "selectionDisabled", false },
			{ "topics", json::array({ { { "index", 0 }, { "topicIndex", 7 }, { "text", "Rumours?" }, { "topicIsNew", true } },
							{ { "index", 1 }, { "topicIndex", 2 }, { "text", "Trade?" }, { "topicIsNew", false } } }) } };
	}

	json Request(const json& a_view)
	{
		return { { "index", 1 }, { "snapshot", DialogueLogic::Snapshot(a_view) } };
	}

	int Error(const json& a_request, const json& a_view)
	{
		try {
			DialogueLogic::ValidateSelection(a_request, a_view);
			return 0;
		} catch (const ToolError& e) {
			return e.code;
		}
	}
}

TEST_CASE("dialogue selection uses list position rather than engine topic index")
{
	const auto view = View();
	CHECK(DialogueLogic::ValidateSelection(Request(view), view) == 1);
	CHECK(view["topics"][1]["topicIndex"] == 2);
}

TEST_CASE("changed speaker or reordered topics cannot receive a stale selection")
{
	auto       view = View();
	const auto request = Request(view);
	view["speaker"] = "Someone else";
	CHECK(Error(request, view) == 409);
	view = View();
	std::swap(view["topics"][0], view["topics"][1]);
	CHECK(Error(request, view) == 409);
	view = View();
	view["topics"][1]["text"] = "Leave";
	CHECK(Error(request, view) == 409);
}

TEST_CASE("progress, transition and hidden menu gates refuse topic selection")
{
	for (const json state : { json(0), json(2), json(3), json(99), json(nullptr) }) {
		auto view = View();
		view["menuState"] = state;
		CHECK(!DialogueLogic::Ready(view));
		CHECK(Error(Request(view), view) == 409);
	}
	for (const auto* field : { "allowProgress", "fadedIn", "listVisible" }) {
		auto view = View();
		view[field] = false;
		CHECK(Error(Request(view), view) == 409);
		view[field] = nullptr;
		CHECK(!DialogueLogic::Ready(view));
	}
	auto view = View();
	view["inputDisabled"] = true;
	CHECK(Error(Request(view), view) == 409);
	view = View();
	view["selectionDisabled"] = true;
	CHECK(Error(Request(view), view) == 409);
}

TEST_CASE("missing menus and unsupported SWFs have distinct refusal reasons")
{
	const auto request = Request(View());
	CHECK(Error(request, { { "open", false } }) == 409);
	CHECK(Error(request, { { "open", true }, { "supported", false } }) == 422);
	CHECK(!DialogueLogic::Ready({ { "open", false } }));
}

TEST_CASE("invalid selection arguments are client errors including unsigned overflow")
{
	const auto view = View();
	for (const auto index : json::array({ -1, 0.5, "1", nullptr, 512, 18446744073709551615ULL })) {
		auto request = Request(view);
		request["index"] = index;
		CHECK(Error(request, view) == 400);
	}
	CHECK(Error({ { "index", 0 } }, view) == 400);
	CHECK(Error({ { "snapshot", DialogueLogic::Snapshot(view) } }, view) == 400);
	CHECK(Error({ { "index", 0 }, { "snapshot", 123 } }, view) == 400);
	CHECK(Error({ { "index", 0 }, { "snapshot", "" } }, view) == 400);
	auto request = Request(view);
	request["index"] = 2;
	CHECK(Error(request, view) == 400);
}

TEST_CASE("snapshot ignores transient subtitles but detects topic content changes")
{
	auto       view = View();
	const auto request = Request(view);
	view["subtitle"] = "Finishing a line";
	view["selectedIndex"] = 0;
	CHECK(Error(request, view) == 0);
	view["topics"][1]["topicIsNew"] = true;
	CHECK(Error(request, view) == 409);
}

TEST_CASE("non-UTF8 UI text can be fingerprinted without throwing")
{
	auto view = View();
	view["speaker"] = std::string("Bj\xF8rn");
	const auto snapshot = DialogueLogic::Snapshot(view);
	CHECK(!snapshot.empty());
	const auto sanitized = json::parse(view.dump(-1, ' ', false, json::error_handler_t::replace));
	CHECK(DialogueLogic::Snapshot(sanitized) == snapshot);
}

TEST_CASE("empty topic lists are observable but cannot be selected")
{
	auto view = View();
	view["topics"] = json::array();
	CHECK(!DialogueLogic::Ready(view));
	CHECK(Error(Request(view), view) == 409);
}

TEST_CASE("unknown dialogue states remain unknown rather than being called stalled")
{
	CHECK(DialogueLogic::StateName(0) == "greeting");
	CHECK(DialogueLogic::StateName(1) == "topics");
	CHECK(DialogueLogic::StateName(2) == "topicClicked");
	CHECK(DialogueLogic::StateName(3) == "transitioning");
	CHECK(DialogueLogic::StateName(9) == "unknown");
	CHECK(DialogueLogic::StateName(nullptr) == "unknown");
}
