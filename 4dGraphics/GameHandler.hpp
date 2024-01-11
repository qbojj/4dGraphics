#pragma once
#include "GameCore.hpp"
#include "GameRenderHandler.hpp"
#include "GameTickHandler.hpp"
#include "cppHelpers.hpp"

namespace v4dg {
class MyGameHandler final : public GameEngine {
public:
  MyGameHandler();
  ~MyGameHandler();

  int Run();
};
} // namespace v4dg