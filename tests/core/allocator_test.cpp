/*******************************************************************************
 * tests/core/allocator_test.cpp
 *
 * Part of Project c7a.
 *
 * Copyright (C) 2015 Timo Bingmann <tb@panthema.net>
 *
 * This file has no license. Only Chuck Norris can compile it.
 ******************************************************************************/

#include <c7a/core/allocator.hpp>
#include <gtest/gtest.h>

#include <deque>
#include <vector>

using namespace c7a;

TEST(NewAllocator, Test1) {
    core::MemoryManager stats(nullptr);

    LOG1 << "vector";
    {
        std::vector<int, core::NewAllocator<int> > my_vector {
            core::NewAllocator<int>(&stats)
        };

        for (size_t i = 0; i < 100; ++i) {
            my_vector.push_back(i);
        }
    }
    LOG1 << "deque";
    {
        std::deque<size_t, core::NewAllocator<size_t> > my_deque {
            core::NewAllocator<size_t>(&stats)
        };

        for (size_t i = 0; i < 100; ++i) {
            my_deque.push_back(i);
        }
    }
}

namespace c7a {
namespace core {

// forced instantiations
template class NewAllocator<int>;

} // namespace core
} // namespace c7a

/******************************************************************************/