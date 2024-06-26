//
// Copyright (c) 2024 Bryan Phillippe
//
// This software is free to use for any purpose, provided this copyright
// notice is preserved.
//

#include <iostream>
#include <sstream>
#include <string>
#include <unistd.h>

#include "stfu.hh"

//
// Self-test the STFU framework or output examples of the API.
//

int
main(int argc, char* argv[])
{
    stfu::test default_result{"default result", []
            {
                stfu::test_result_data d;

                STFU_PASS_IFF(stfu::test_result::DIDNT_RUN == d.result);
            },
            "Verify that the default result for a test_result_data is "
            "DIDNT_RUN."
    };

    stfu::test default_values{"default values", []
            {
                stfu::test t{"", []{}};

                STFU_PASS_IFF(t.is_enabled());
            },
            "Verify that tests are enabled by default."
    };

    stfu::test enable_disable{"enable/disable", []
            {
                stfu::test t{"", []{}};

                STFU_ASSERT(t.is_enabled());
                STFU_ASSERT(!t.set_enable(false).is_enabled());
                STFU_ASSERT(t.set_enable(true).is_enabled());

                STFU_PASS();
            },
            "Verify the ability to enable and disable tests."
    };

    stfu::test basic_skipped{"basic skipped", []
            {
                stfu::test t{"", []{}};

                t.set_enable(false);

                STFU_PASS_IFF(stfu::test_result::SKIPPED == t().result);
            },
            "Verify that disabling a test causes it to be skipped."
    };

    stfu::test basic_pass{"basic pass", []
            {
                stfu::test t{"", []{ STFU_PASS(); }};

                STFU_PASS_IFF(stfu::test_result::PASS == t().result);
            },
            "Verify a simple passing test result."
    };

    stfu::test basic_fail{"basic fail", []
            {
                stfu::test t{"", []{}};

                STFU_PASS_IFF(stfu::test_result::FAIL == t().result);
            },
            "Verify a simple failing test result."
    };

    stfu::test basic_crash{"basic crash", []
            {
                stfu::test t{"", []{ *((int*)0x1) = 1; }};
                auto r = t();

                STFU_PASS_IFF(stfu::test_result::CRASH == r.result);
            },
            "Verify a simple crashing test result. For this test we will "
            "force a segmentation violation with an invalid memory access."
    };

    stfu::test name_test{"test name", []
            {
                stfu::test t{"SomeValue", []{}};

                STFU_PASS_IFF(t.get_name() == "SomeValue" &&
                              t.get_name() != "WrongValue");
            },
            "Verify that test name can be set."
    };

    stfu::test description_test{"test description", []
            {
                stfu::test t{"", []{}, "My Value"};

                STFU_PASS_IFF(t.get_description() == "My Value" &&
                              t.get_description() != "WrongValue");

            },
            "Verify that test description can be set."
    };

    auto substr_count = [](const std::string& s, const std::string& p){
        std::size_t count = 0;
        decltype(s.size()) pos = 0;
        while (std::string::npos != (pos = s.find(p, pos))) {
            ++count;
            pos += p.length();
        }
        return count;
    };

    stfu::test fixtures{"fixtures count", [substr_count]
            {
                size_t fixture_in = 0, fixture_out = 0;

                stfu::test_group nested{"nested", "nested tests"};

                nested.add_test(stfu::test{"(fixtures 1...)",
                                [&](){ STFU_PASS_IFF(2 == fixture_in &&
                                                     0 == fixture_out); }})
                      .add_test(stfu::test{"(fixtures 2...)",
                                [&](){ STFU_PASS_IFF(3 == fixture_in &&
                                                     1 == fixture_out); }})
                      .add_test(stfu::test{"(fixtures 3...)",
                                [&](){ STFU_PASS_IFF(4 == fixture_in &&
                                                     2 == fixture_out); }})
                      .add_before_all(stfu::test_group::fixture{
                                [&](){ ++fixture_in; return true; }})
                      .add_before_each(stfu::test_group::fixture{
                                [&](){ ++fixture_in; return true; }})
                      .add_after_all(stfu::test_group::fixture{
                                [&](){ ++fixture_out; return true; }})
                      .add_after_each(stfu::test_group::fixture{
                                [&](){ ++fixture_out; return true; }})
                      .set_verbose(false);

                std::ostringstream output;
                stfu::test_result_summary summary = nested(output);
                STFU_ASSERT(0 == summary.didnt_run);
                STFU_ASSERT(0 == summary.skipped);
                STFU_ASSERT(3 == summary.passed);
                STFU_ASSERT(0 == summary.failed);
                STFU_ASSERT(0 == summary.crashed);
                STFU_ASSERT(4 == fixture_in && 4 == fixture_out);
                STFU_PASS_IFF(3 == substr_count(output.str(), "PASS"));
            },
            "Verify that fixtures fire when they should."
    };

    stfu::test fixtures_errors{"fixtures errors", []
            {
                stfu::test_group nested{"nested", "nested tests"};

                nested.add_test(stfu::test{"(fixtures)",
                                [](){ STFU_FAIL(); }})
                      .add_before_all(stfu::test_group::fixture{
                                [](){ return false; }})
                      .set_verbose(false);

                std::ostringstream output;
                stfu::test_result_summary summary = nested(output);
                STFU_ASSERT(1 == summary.didnt_run);
                STFU_ASSERT(0 == summary.skipped);
                STFU_ASSERT(0 == summary.passed);
                STFU_ASSERT(0 == summary.failed);
                STFU_ASSERT(0 == summary.crashed);
                STFU_PASS_IFF("# ERROR - failure in fixture: before_all\n" ==
                              output.str());
            },
            "Verify behavior of exceptional fixtures."
    };

    //
    // Group of all the unit tests (all expected to PASS).
    //

    stfu::test_group unit_tests{"unit tests",
                                "Self-tests of the STFU public API."};

    unit_tests.add_test(default_result)
              .add_test(default_values)
              .add_test(enable_disable)
              .add_test(basic_skipped)
              .add_test(basic_pass)
              .add_test(basic_fail)
              .add_test(basic_crash)
              .add_test(name_test)
              .add_test(description_test)
              .add_test(stfu::test{
                  "anonymous test",
                  []{ STFU_PASS(); },
                  "Anonymously defined test"})
              .add_test(fixtures)
              .add_test(fixtures_errors)
              .set_verbose(false);

    //
    // Examples
    //

    stfu::test explicit_failure{"explicit failure", []
        {
            STFU_FAIL();
        },
        "Demonstration of an explicit failure by calling STFU_FAIL(). The "
        "output will include the filename and line number where the failure "
        "occurred."
    };

    stfu::test implicit_failure{"implicit failure", []
        {
        },
        "Demonstration of an implicit failure, where no code actually runs. "
        "This is considered a failure by the STFU framework, as we prohibit "
        "implicitly successful test runs - a test can only pass if it "
        "explicitly calls STFU_PASS() or STFU_PASS_IFF().\n\n"
        "Note that no location information is provided in this case."
    };

    stfu::test skipped{"skipped test", []
        {
        },
        "Demonstration of a test that is skipped because it's disabled. This "
        "is useful for temporarily turning off tests without removing them "
        "from the test run output."
    };

    stfu::test slow_test{"slow test", []
        {
            ::sleep(1);
            STFU_PASS();
        },
        "Demonstration of a test that takes slightly longer to run. Use this "
        "to compare the runtime reported for a test vs. those that complete "
        "without delay."
    };

    stfu::test pass_iff{"pass iff", []
        {
            STFU_PASS_IFF(1 == 0);
        },
        "Demonstration of the STFU_PASS_IFF() macro, which will immediately "
        "pass the test if and only if the expression is true. Otherwise, it "
        "fails immediately."
    };

    stfu::test failed_assertion{"assertion", []
        {
            STFU_ASSERT(0 == 1);
        },
        "Demonstration of the case of a failed assertion via "
        "STFU_ASSERT(0 == 1). In addition to the filename and line number, "
        "it will also include the expression which failed to evaluate to true."
    };

    stfu::test crash_case{"segfault condition", []
        {
            STFU_ASSERT(0xdead == *(int*)0x1);
        },
        "Demonstration of how an invalid memory access which leads to a "
        "segmentation fault will appear as a test failure."
    };

    stfu::test_group examples{"examples",
        "Examples of various uses and failure conditions."};

    //
    // Group of example tests (mix of failures).
    //
    examples.add_test(explicit_failure)
            .add_test(implicit_failure)
            .add_test(skipped.set_enable(false))
            .add_test(slow_test)
            .add_test(pass_iff)
            .add_test(failed_assertion)
            .add_test(crash_case)
            .set_verbose(true);

    if (1 == argc) {
        stfu::test_result_summary summary = unit_tests();
        return static_cast<int>(summary.failed + summary.crashed);
    }

    std::string arg{argv[1]};
    if (arg == "--examples") {
        examples();
        return 0;
    }

    std::cerr << "Usage: " << argv[0] << " [--examples]" << std::endl;

    if (arg == "--help") {
        return 0;
    }

    return -1;
}
