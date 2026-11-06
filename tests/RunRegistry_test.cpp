#include "test_framework.h"

#include "RunRegistry.h"

using dvb::json;
using dvb::RunRegistry;

TEST_CASE("RunRegistry sequence assigns monotonic unique runIds")
{
	auto& reg = RunRegistry::Get();
	reg.ClearForTesting();

	const uint64_t id1 = reg.NextId();
	const uint64_t id2 = reg.NextId();
	CHECK(id1 == 1);
	CHECK(id2 == 2);
}

TEST_CASE("RunRegistry tracks in-flight and finished runs")
{
	auto& reg = RunRegistry::Get();
	reg.ClearForTesting();

	const uint64_t id = reg.NextId();
	reg.Start(id);

	auto st = reg.Status(id);
	CHECK(st.has_value());
	CHECK(st->value("done", true) == false);
	CHECK(st->value("runId", 0) == id);

	reg.Finish(id, json{ { "ok", true }, { "stepsRun", 5 } });
	st = reg.Status(id);
	CHECK(st.has_value());
	CHECK(st->value("done", false) == true);
	CHECK(st->value("ok", false) == true);
	CHECK(st->contains("result"));
	CHECK((*st)["result"].value("stepsRun", 0) == 5);
}

TEST_CASE("RunRegistry cancellation marks run cancelled and updates status")
{
	auto& reg = RunRegistry::Get();
	reg.ClearForTesting();

	// Unknown run
	CHECK(reg.Cancel(999) == RunRegistry::CancelResult::NotFound);
	CHECK(!reg.IsCancelled(999));

	const uint64_t id = reg.NextId();
	reg.Start(id);
	CHECK(!reg.IsCancelled(id));

	// Cancel in flight
	CHECK(reg.Cancel(id) == RunRegistry::CancelResult::Cancelled);
	CHECK(reg.IsCancelled(id));

	// Status while in flight reflects cancelling
	auto st = reg.Status(id);
	CHECK(st.has_value());
	CHECK(st->value("done", true) == false);
	CHECK(st->value("cancelling", false) == true);

	// When worker finishes a cancelled run, ok is forced false and cancelled is true
	reg.Finish(id, json{ { "ok", true }, { "cancelled", true }, { "stepsRun", 2 } });
	st = reg.Status(id);
	CHECK(st.has_value());
	CHECK(st->value("done", false) == true);
	CHECK(st->value("ok", true) == false);
	CHECK(st->value("cancelled", false) == true);

	// Trying to cancel after completion returns AlreadyDone
	CHECK(reg.Cancel(id) == RunRegistry::CancelResult::AlreadyDone);
}

TEST_CASE("RunRegistry fails with error output")
{
	auto& reg = RunRegistry::Get();
	reg.ClearForTesting();

	const uint64_t id = reg.NextId();
	reg.Start(id);
	reg.Fail(id, "unexpected failure");

	auto st = reg.Status(id);
	CHECK(st.has_value());
	CHECK(st->value("done", false) == true);
	CHECK(st->value("ok", true) == false);
	CHECK(st->value("error", "") == "unexpected failure");
}
