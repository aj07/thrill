/*******************************************************************************
 * tests/api/zip_node_test.cpp
 *
 * Part of Project c7a.
 *
 * Copyright (C) 2015 Timo Bingmann <tb@panthema.net>
 *
 * This file has no license. Only Chuck Norris can compile it.
 ******************************************************************************/

#include <c7a/api/allgather.hpp>
#include <c7a/api/bootstrap.hpp>
#include <c7a/api/generate.hpp>
#include <c7a/api/lop_node.hpp>
#include <c7a/api/size.hpp>
#include <c7a/api/zip.hpp>
#include <c7a/common/string.hpp>
#include <c7a/data/serialization_cereal.hpp>
#include <gtest/gtest.h>

#include <algorithm>
#include <random>
#include <string>

using namespace c7a;

using c7a::api::Context;
using c7a::api::DIARef;

struct MyStruct {
    int a, b;

    MyStruct() { }
    MyStruct(int _a, int _b) : a(_a), b(_b) { }

    // This method lets cereal know which data members to serialize
    template <class Archive>
    void serialize(Archive& archive) {
        archive(a, b);
    }
};

// namespace c7a {
// namespace data {

// template <typename Archive>
// struct Impl<Archive, MyStruct>
// {
//     static void Serialize(const MyStruct& x, Archive& ar) {
//         Impl<Archive, int>::Serialize(x.a, ar);
//         Impl<Archive, int>::Serialize(x.b, ar);
//     }
//     static MyStruct Deserialize(Archive& ar) {
//         int a = Impl<Archive, int>::Deserialize(ar);
//         int b = Impl<Archive, int>::Deserialize(ar);
//         return MyStruct(a, b);
//     }
//     static const bool fixed_size = (Impl<Archive, int>::fixed_size &&
//                                     Impl<Archive, int>::fixed_size);
// };

// } // namespace data
// } // namespace c7a

static const size_t test_size = 1000;

TEST(ZipNode, TwoBalancedIntegerArrays) {

    std::function<void(Context&)> start_func =
        [](Context& ctx) {

            // numbers 0..999 (evenly distributed to workers)
            auto zip_input1 = Generate(
                ctx,
                [](size_t index) { return index; },
                test_size);

            // numbers 1000..1999
            auto zip_input2 = zip_input1.Map(
                [](size_t i) { return static_cast<short>(test_size + i); });

            // zip
            auto zip_result = zip_input1.Zip(
                zip_input2, [](size_t a, short b) -> long { return a + b; });

            // check result
            std::vector<long> res = zip_result.AllGather();

            ASSERT_EQ(test_size, res.size());

            for (size_t i = 0; i != res.size(); ++i) {
                ASSERT_EQ(static_cast<long>(i + i + test_size), res[i]);
            }
        };

    // c7a::api::ExecuteLocalThreadsTCP(1, 8080, start_func);
    c7a::api::ExecuteLocalTests(start_func);
}

TEST(ZipNode, TwoDisbalancedIntegerArrays) {

    // first DIA is heavily balanced to the first workers, second DIA is
    // balanced to the last workers.
    std::function<void(Context&)> start_func =
        [](Context& ctx) {

            // numbers 0..999 (evenly distributed to workers)
            auto input1 = Generate(
                ctx,
                [](size_t index) { return index; },
                test_size);

            // numbers 1000..1999
            auto input2 = input1.Map(
                [](size_t i) { return static_cast<short>(test_size + i); });

            // numbers 0..99 (concentrated on first workers)
            auto zip_input1 = input1.Filter(
                [](size_t i) { return i < test_size / 10; });

            // numbers 1900..1999 (concentrated on last workers)
            auto zip_input2 = input2.Filter(
                [](size_t i) { return i >= 2 * test_size - test_size / 10; });

            // zip
            auto zip_result = zip_input1.Zip(
                zip_input2, [](size_t a, short b) -> MyStruct { return MyStruct(a, b); });

            // check result
            std::vector<MyStruct> res = zip_result.AllGather();

            ASSERT_EQ(test_size / 10, res.size());

            for (size_t i = 0; i != res.size(); ++i) {
                //sLOG1 << i << res[i].a << res[i].b;
                ASSERT_EQ(static_cast<long>(i), res[i].a);
                ASSERT_EQ(static_cast<long>(2 * test_size - test_size / 10 + i), res[i].b);
            }

            // TODO(sl): make this work!
            // check size of zip (recalculates ZipNode)
            ASSERT_EQ(100u, zip_result.Size());
        };

    // c7a::api::ExecuteLocalThreadsTCP(1, 8080, start_func);
    c7a::api::ExecuteLocalTests(start_func);
}

TEST(ZipNode, TwoIntegerArraysWhereOneIsEmpty) {

    std::function<void(Context&)> start_func =
        [](Context& ctx) {

            // numbers 0..999 (evenly distributed to workers)
            auto input1 = Generate(
                ctx,
                [](size_t index) { return index; },
                test_size);

            // numbers 0..999 (evenly distributed to workers)
            auto input2 = Generate(
                ctx,
                [](size_t index) { return index; },
                0);

            // zip
            auto zip_result = input1.Zip(
                input2, [](size_t a, short b) -> long { return a + b; });

            // check result
            std::vector<long> res = zip_result.AllGather();
            ASSERT_EQ(0u, res.size());
        };

    // c7a::api::ExecuteLocalThreadsTCP(1, 8080, start_func);
    c7a::api::ExecuteLocalTests(start_func);
}

TEST(ZipNode, TwoDisbalancedStringArrays) {

    // first DIA is heavily balanced to the first workers, second DIA is
    // balanced to the last workers.
    std::function<void(Context&)> start_func =
        [](Context& ctx) {

            // generate random strings with 10..20 characters
            auto input_gen = Generate(
                ctx,
                [](size_t index) -> std::string {
                    std::default_random_engine rng(123456 + index);
                    std::uniform_int_distribution<int> length(10, 20);
                    rng(); // skip one number

                    return common::RandomString(
                        length(rng), rng, "abcdefghijklmnopqrstuvwxyz")
                    + std::to_string(index);
                },
                test_size);

            DIARef<std::string> input = input_gen.Collapse();

            std::vector<std::string> vinput = input.AllGather();
            ASSERT_EQ(test_size, vinput.size());

            // Filter out strings that start with a-e
            auto input1 = input.Filter(
                [](const std::string& str) { return str[0] <= 'e'; });

            // Filter out strings that start with w-z
            auto input2 = input.Filter(
                [](const std::string& str) { return str[0] >= 'w'; });

            // zip
            auto zip_result = input1.Zip(
                input2, [](const std::string& a, const std::string& b) {
                    return a + b;
                });

            // check result
            std::vector<std::string> res = zip_result.AllGather();

            // recalculate result locally
            std::vector<std::string> check;
            {
                std::vector<std::string> v1, v2;

                for (size_t index = 0; index < vinput.size(); ++index) {
                    const std::string& s1 = vinput[index];
                    if (s1[0] <= 'e') v1.push_back(s1);
                    if (s1[0] >= 'w') v2.push_back(s1);
                }

                ASSERT_EQ(v1, input1.AllGather());
                ASSERT_EQ(v2, input2.AllGather());

                for (size_t i = 0; i < std::min(v1.size(), v2.size()); ++i) {
                    check.push_back(v1[i] + v2[i]);
                    //sLOG1 << check.back();
                }
            }

            for (size_t i = 0; i != res.size(); ++i) {
                sLOG0 << res[i] << " " << check[i] << (res[i] == check[i]);
            }

            ASSERT_EQ(check.size(), res.size());
            ASSERT_EQ(check, res);
        };

    //c7a::api::ExecuteLocalThreadsTCP(2, 8080, start_func);
    c7a::api::ExecuteLocalTests(start_func);
}

/******************************************************************************/