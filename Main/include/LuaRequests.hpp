#pragma once

struct AsyncRequest
{
	struct lua_State* L;
	cpr::AsyncResponse r;
	int callback;

	AsyncRequest(struct lua_State* luaState, cpr::AsyncResponse asyncResponse, int callback)
		: L(luaState), r(std::move(asyncResponse)), callback(callback)
	{
	}
};

struct CompleteRequest
{
	struct lua_State* L;
	cpr::Response r;
	int callback;
};
