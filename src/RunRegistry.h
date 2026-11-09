#pragma once

#include <atomic>
#include <cstdint>
#include <map>
#include <mutex>
#include <optional>
#include <string>

#include "Json.h"

namespace dvb
{
	// Tracks in-flight/completed async runs — record{action:"replay"} (async by default) and
	// scenario{action:"run", async:true} share this registry and its runId space, so a runId
	// from either polls correctly via either tool's action="status". Entries are pruned once
	// the retention cap is hit so a caller that never polls a finished run doesn't leak state.
	class RunRegistry
	{
	public:
		enum class CancelResult
		{
			NotFound,
			AlreadyDone,
			Cancelled
		};

		static RunRegistry& Get()
		{
			static RunRegistry inst;
			return inst;
		}

		uint64_t NextId() { return ++m_seq; }

		void Start(uint64_t a_runId)
		{
			std::lock_guard lock(m_mtx);
			m_runs.emplace(a_runId, State{});
			Prune();
		}

		// `st.ok` reflects the result's own "ok" field when present (a scenario/replay run
		// that completed without throwing but failed an assertion has result.ok=false — that
		// must not be reported as a top-level ok:true) rather than just "didn't throw".
		void Finish(uint64_t a_runId, json a_result)
		{
			std::lock_guard lock(m_mtx);
			State&          st = m_runs[a_runId];
			st.done = true;
			st.cancelled = st.cancelled || (a_result.is_object() && a_result.value("cancelled", false));
			st.ok = st.cancelled ? false : (a_result.is_object() ? a_result.value("ok", true) : true);
			st.result = std::move(a_result);
			st.hasResult = true;
		}

		void Fail(uint64_t a_runId, std::string a_error)
		{
			std::lock_guard lock(m_mtx);
			State&          st = m_runs[a_runId];
			st.done = true;
			st.ok = false;
			st.error = std::move(a_error);
		}

		CancelResult Cancel(uint64_t a_runId)
		{
			std::lock_guard lock(m_mtx);
			const auto      it = m_runs.find(a_runId);
			if (it == m_runs.end())
				return CancelResult::NotFound;
			if (it->second.done)
				return CancelResult::AlreadyDone;
			it->second.cancelled = true;
			return CancelResult::Cancelled;
		}

		bool IsCancelled(uint64_t a_runId)
		{
			std::lock_guard lock(m_mtx);
			const auto      it = m_runs.find(a_runId);
			return it != m_runs.end() && it->second.cancelled;
		}

		// hasResult (Finish, even with ok:false — a failed assertion, not a thrown exception)
		// vs error-only (Fail — the run itself threw) are distinct outcomes; conflating them
		// on `ok` alone would discard the actual transcript for a failed-but-completed run.
		std::optional<json> Status(uint64_t a_runId)
		{
			std::lock_guard lock(m_mtx);
			const auto      it = m_runs.find(a_runId);
			if (it == m_runs.end())
				return std::nullopt;
			const State& st = it->second;
			if (!st.done) {
				json out{ { "runId", a_runId }, { "done", false } };
				if (st.cancelled)
					out["cancelling"] = true;
				return out;
			}
			json out{ { "runId", a_runId }, { "done", true }, { "ok", st.ok } };
			if (st.cancelled)
				out["cancelled"] = true;
			out[st.hasResult ? "result" : "error"] = st.hasResult ? st.result : json(st.error);
			return out;
		}

		/// Every retained run, oldest first, without transcripts.
		json List()
		{
			std::lock_guard lock(m_mtx);
			json            out = json::array();
			for (const auto& [id, st] : m_runs) {
				json entry{ { "runId", id }, { "done", st.done } };
				if (st.done)
					entry["ok"] = st.ok;
				if (st.cancelled)
					entry["cancelled"] = true;
				out.push_back(std::move(entry));
			}
			return out;
		}

		void ClearForTesting()
		{
			std::lock_guard lock(m_mtx);
			m_runs.clear();
			m_seq.store(0);
		}

	private:
		struct State
		{
			bool        done = false;
			bool        ok = false;
			bool        cancelled = false;
			bool        hasResult = false;
			json        result;
			std::string error;
		};

		// runId is a monotonic counter, so the smallest key is the oldest run. Never erase an
		// in-flight (!done) one — Finish/Fail would recreate it via operator[], and a poll in
		// between would 404 on a run that's actually still executing.
		void Prune()
		{
			constexpr size_t kRetention = 128;  // shared by two tools now; headroom over the old 64
			for (auto it = m_runs.begin(); m_runs.size() > kRetention && it != m_runs.end();)
				it = it->second.done ? m_runs.erase(it) : std::next(it);
		}

		std::atomic<uint64_t>     m_seq{ 0 };
		std::mutex                m_mtx;
		std::map<uint64_t, State> m_runs;
	};
}
