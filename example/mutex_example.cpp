#pragma once
#include <coco/runtime.hpp>
#include <coco/sync.hpp>

static coco::Runtime rt(8);

std::size_t counter = 0;

auto main() -> int
{
  rt.block([]() -> coco::Task<> {
    
    co_return; 
  });
}