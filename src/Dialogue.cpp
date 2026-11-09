#include "Dialogue.h"

#include "DialogueLogic.h"
#include "MainThread.h"

#include <cmath>
#include <set>

namespace dvb::Dialogue
{
	namespace
	{
		RE::GFxValue Member(const RE::GFxValue& a_object, const char* a_name)
		{
			RE::GFxValue value;
			if (a_object.IsObject())
				a_object.GetMember(a_name, &value);
			return value;
		}

		json Scalar(const RE::GFxValue& a_value)
		{
			if (a_value.IsBool())
				return a_value.GetBool();
			if (a_value.IsNumber() && std::isfinite(a_value.GetNumber()))
				return a_value.GetNumber();
			if (a_value.IsString())
				return a_value.GetString() ? a_value.GetString() : "";
			if (a_value.GetType() == RE::GFxValue::ValueType::kStringW)
				return a_value.GetStringW() ? SKSE::stl::utf16_to_utf8(a_value.GetStringW()).value_or("") : "";
			return nullptr;
		}

		bool TopicIndex(const json& a_value)
		{
			if (!a_value.is_number())
				return false;
			const double value = a_value.get<double>();
			return std::isfinite(value) && value >= 0 && value <= 2147483647 && std::floor(value) == value;
		}

		json Inspect(RE::GFxValue& a_root, RE::GFxValue& a_list, RE::GFxMovieView*& a_movie)
		{
			json  out = { { "ok", true }, { "open", false }, { "supported", false }, { "ready", false }, { "topics", json::array() } };
			auto* ui = RE::UI::GetSingleton();
			if (!ui || !ui->IsMenuOpen(RE::DialogueMenu::MENU_NAME))
				return out;
			const auto menu = ui->GetMenu(RE::DialogueMenu::MENU_NAME);
			if (!menu || !menu->uiMovie)
				return out;
			out["open"] = true;
			a_movie = menu->uiMovie.get();
			if (!a_movie->GetVariable(&a_root, "_root.DialogueMenu_mc") || !a_root.IsObject()) {
				out["reason"] = "missing _root.DialogueMenu_mc";
				return out;
			}
			a_list = Member(a_root, "TopicList");
			const auto holder = Member(a_root, "TopicListHolder");
			out["speaker"] = Scalar(Member(Member(a_root, "SpeakerName"), "text"));
			out["subtitle"] = Scalar(Member(Member(a_root, "SubtitleText"), "text"));
			out["menuState"] = Scalar(Member(a_root, "eMenuState"));
			out["state"] = DialogueLogic::StateName(out["menuState"]);
			out["allowProgress"] = Scalar(Member(a_root, "bAllowProgress"));
			out["fadedIn"] = Scalar(Member(a_root, "bFadedIn"));
			out["listVisible"] = Scalar(Member(holder, "_visible"));
			out["inputDisabled"] = Scalar(Member(a_list, "bDisableInput"));
			out["selectionDisabled"] = Scalar(Member(a_list, "bDisableSelection"));
			out["selectedIndex"] = Scalar(Member(a_list, "selectedIndex"));
			auto entries = Member(a_list, "entryList");
			if (!entries.IsArray())
				entries = Member(a_list, "EntriesA");
			if (!entries.IsArray() || entries.GetArraySize() > 512) {
				out["reason"] = "missing topic array or more than 512 topics";
				return out;
			}
			std::set<int> indexes;
			bool          valid = a_list.IsObject() && a_list.HasMember("SetSelectedTopic") && a_list.HasMember("UpdateList") && a_root.HasMember("onItemSelect") &&
			                      out["speaker"].is_string() && out["menuState"].is_number() &&
			                      out["allowProgress"].is_boolean() && out["fadedIn"].is_boolean() &&
			                      out["listVisible"].is_boolean() && out["inputDisabled"].is_boolean() && out["selectionDisabled"].is_boolean();
			for (std::uint32_t i = 0; i < entries.GetArraySize(); ++i) {
				RE::GFxValue entry;
				entries.GetElement(i, &entry);
				json topic = { { "index", i }, { "text", Scalar(Member(entry, "text")) },
					{ "topicIndex", Scalar(Member(entry, "topicIndex")) }, { "topicIsNew", Scalar(Member(entry, "topicIsNew")) } };
				if (!topic["text"].is_string() || !TopicIndex(topic["topicIndex"])) {
					valid = false;
				} else if (!indexes.insert(topic["topicIndex"].get<int>()).second) {
					valid = false;
				}
				out["topics"].push_back(std::move(topic));
			}
			out["supported"] = valid;
			out["ready"] = DialogueLogic::Ready(out);
			if (valid)
				out["snapshot"] = DialogueLogic::Snapshot(out);
			else
				out["reason"] = "missing or malformed dialogue fields, or duplicate engine topic indexes";
			return out;
		}

		json Run(const json& a_args)
		{
			RE::GFxValue      root, list;
			RE::GFxMovieView* movie = nullptr;
			json              view = Inspect(root, list, movie);
			if (a_args.value("action", std::string("inspect")) == "inspect")
				return view;
			const auto index = DialogueLogic::ValidateSelection(a_args, view);
			if (!list.HasMember("SetSelectedTopic") || !list.HasMember("UpdateList") || !root.HasMember("onItemSelect"))
				throw ToolError(422, "Dialogue Menu selection methods are unavailable");
			RE::GFxValue topic, result;
			topic.SetNumber(view["topics"][index]["topicIndex"].get<double>());
			if (!list.Invoke("SetSelectedTopic", &result, &topic, 1))
				throw ToolError(422, "SetSelectedTopic failed; topic was not submitted");
			if (!list.Invoke("UpdateList", &result, nullptr, 0))
				throw ToolError(422, "UpdateList failed; topic was not submitted");
			if (Scalar(Member(list, "selectedIndex")) != json(index) || Scalar(Member(Member(list, "selectedEntry"), "topicIndex")) != view["topics"][index]["topicIndex"])
				throw ToolError(422, "menu did not select the requested topic; topic was not submitted");
			RE::GFxValue event, keyboard, mouse;
			movie->CreateObject(&event);
			keyboard.SetNumber(1);
			mouse.SetBoolean(false);
			event.SetMember("keyboardOrMouse", keyboard);
			event.SetMember("mouseClick", mouse);
			// Use the active SWF's input handler so UI mods retain their own topic-click behaviour.
			if (!root.Invoke("onItemSelect", &result, &event, 1))
				throw ToolError(422, "onItemSelect invocation failed; inspect before retrying");
			return { { "ok", true }, { "submitted", true }, { "completed", false }, { "topic", view["topics"][index] } };
		}
	}

	ToolDescriptor BuildDescriptor()
	{
		ToolDescriptor descriptor;
		descriptor.name = "dialogue";
		descriptor.description =
			"Inspect the active conversation or select a dialogue topic through the active SWF's normal input handler. "
			"inspect (default) returns open, supported, ready, speaker, subtitle, menuState/state, progress flags, selectedIndex, "
			"topics [{index,text,topicIndex,topicIsNew}] and snapshot. select requires a zero-based list index and the snapshot "
			"from inspect; changed speaker/topics and busy menus are refused. A snapshot checks observed content, not session identity. "
			"submitted means the UI handler was invoked, not that the response or gameplay completed; inspect again to observe progress. "
			"Supports vanilla-compatible DialogueMenu_mc interfaces; custom layouts report supported=false. topicIsNew is raw UI data, "
			"not a guarantee of displayed colour or persistent heard state. No mod-specific callbacks are bypassed.";
		descriptor.inputSchema = {
			{ "type", "object" },
			{ "properties", { { "action", { { "type", "string" }, { "enum", json::array({ "inspect", "select" }) } } },
								{ "index", { { "type", "integer" }, { "minimum", 0 }, { "maximum", 511 }, { "description", "select: zero-based index from topics, not engine topicIndex" } } },
								{ "snapshot", { { "type", "string" }, { "description", "select: snapshot returned by dialogue inspect" } } } } }
		};
		return descriptor;
	}

	json Handle(const json& a_args, const ToolContext&)
	{
		if (!a_args.is_object() || (a_args.contains("action") && !a_args["action"].is_string()))
			throw ToolError(400, "action must be inspect or select");
		const auto action = a_args.value("action", std::string("inspect"));
		if (action != "inspect" && action != "select")
			throw ToolError(400, "action must be inspect or select");
		json result = MainThread::RunAndWait([args = a_args]() { return Run(args); }, std::chrono::milliseconds(5000), nullptr, MainThread::Queue::kUi);
		return json::parse(result.dump(-1, ' ', false, json::error_handler_t::replace));
	}
}
