#pragma once

#include <queue>
#include <memory>

namespace GQueue
{
	struct Command;
	typedef void *ObjectID;

	typedef void (*CommandEvaluator)(const Command *self);

	struct Command
	{
		CommandEvaluator eval;
		uint64_t user[4];
	};

	namespace detail
	{
		void CreateTexture( const Command *self );
	};

	typedef std::queue<Command> CommandQueue;
}