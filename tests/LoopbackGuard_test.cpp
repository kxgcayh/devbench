// The REST/MCP port must refuse browser-driven requests (cross-site POST, DNS rebinding)
// while still accepting header-less local clients (curl, Node, Python).

#include "test_framework.h"

#include "LoopbackGuard.h"

using dvb::IsLocalRequest;

TEST_CASE("local clients without Origin are accepted")
{
	CHECK(IsLocalRequest("127.0.0.1:8920", ""));
	CHECK(IsLocalRequest("localhost:8921", ""));
	CHECK(IsLocalRequest("[::1]:8920", ""));
}

TEST_CASE("a foreign Host (DNS rebinding) is refused")
{
	CHECK(!IsLocalRequest("evil.example:8920", ""));
	CHECK(!IsLocalRequest("127.0.0.1.evil.example:8920", ""));
	CHECK(!IsLocalRequest("", ""));
}

TEST_CASE("a cross-site or opaque Origin is refused even with a loopback Host")
{
	CHECK(!IsLocalRequest("127.0.0.1:8920", "https://evil.example"));
	CHECK(!IsLocalRequest("127.0.0.1:8920", "null"));
	CHECK(IsLocalRequest("127.0.0.1:8920", "http://localhost:8920"));
}
