#pragma once

#include <string_view>

namespace dvb
{
	/// "127.0.0.1:8920", "localhost", "[::1]:8920" -> true. Anything else (other hosts, "null") -> false.
	inline bool IsLoopbackAuthority(std::string_view a_authority)
	{
		if (a_authority.starts_with('['))
			return a_authority.starts_with("[::1]");
		const auto host = a_authority.substr(0, a_authority.find(':'));
		return host == "127.0.0.1" || host == "localhost";
	}

	/// Gate for the REST/MCP port. A web page can POST text/plain to 127.0.0.1 without a CORS
	/// preflight, and DNS rebinding swaps the Host header, so both headers must be loopback.
	/// curl, Node fetch and Python send no Origin and pass.
	inline bool IsLocalRequest(std::string_view a_host, std::string_view a_origin)
	{
		if (!IsLoopbackAuthority(a_host))
			return false;
		if (a_origin.empty())
			return true;
		const auto scheme = a_origin.find("://");
		return scheme != std::string_view::npos && IsLoopbackAuthority(a_origin.substr(scheme + 3));
	}
}
