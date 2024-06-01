# Simple Test Framework for UNIX

A simple, single-header unit-test framework for UNIX-like operating systems, written in C++.

*STFU*, for short 😜, aims to allow for rapid adoption and development of unit tests for C++ applications, by requiring only a single header to be included, and no libraries/linking or special compilation flags.

Tests are defined as blocks of code that are then added to test groups, and groups are invoked as functional objects. Individual tests are run as a subprocess, enabling crash-detection and address-space compartmentalization. In order for any test to "pass", it must do so explicitly - simply returning 0 or true (or not running at all) will **not** yield a passing result.

Optional fixtures (see below) may be added to test execution to assist with setup.

Test results - including wall-clock runtime - are printed to an output stream (standard out by default) in a parser-friendly format suitable for integration with scripts/Make-based build projects.

## Usage

Include `stfu.hh` in your application, then compile as usual. Note that the STFU framework is written in C++11, and therefore you may need to explicitly define this via `--std=c++11` or equivalent in your compilation flags.

Tests are defined via the `stfu::test` class, added to a `stfu::test_group`, and then invoked when the group is invoked as a function. Code blocks are wraped into the test class as a closure, and then call macros such as `STFU_PASS()` or `STFU_FAIL()` to indicate pass or fail results in the logical fashion.

Test groups may be configured for verbose output (in which case descriptions of the tests will be printed along with their results), and individual tests may be disabled as a means of "commenting them out."

A simple example is shown below:

```
stfu::test example_test{"Test name", []
    {
        STFU_ASSERT(1 == 1);
        STFU_PASS();
    },
    "A description of this example test, showing how a true assertion allows "
    "the code to continue to the final line, which concludes with the test "
    "passing"
};

stfu::test_group example_group{"Group name",
    "Example of a group with a single test added to show the syntax of STFU."};

example_group.add_test(example_test)
             .set_verbose(true); // Turn on detailed output

example_group();
```

The above code would result in the following output:

```
# Running 1 test(s) in group: Group name
#
# Example of a group with a single test added to show the syntax of STFU.
#
# Test name: 
#   A description of this example test, showing how a true assertion allows
#   the code to continue to the final line, which concludes with the test
#   passing
#   
Test name           PASS - in 0.00173667s

# Summary: Group name completed with 0 failures
```

## General Examples

A full set of examples are included in the STFU unit-test program, `test.cc`. Build and run this with argument `--examples` to see how test cases will print for various conditions. View the code itself to see how the examples work.

```
% make
c++ --std=c++11 -Wall -MMD -MP -c test.cc -o test.o
c++ test.o -o selftest
% ./selftest --examples
# Running 7 test(s) in group: examples
#
# Examples of various uses and failure conditions.
#
# explicit failure: 
#   Demonstration of an explicit failure by calling STFU_FAIL(). The output
#   will include the filename and line number where the failure occurred.
#   
explicit failure    FAIL (FAILED at test.cc:137) - in 0.00144908s

# implicit failure: 
#   Demonstration of an implicit failure, where no code actually runs. This is
#   considered a failure by the STFU framework, as we prohibit implicitly
#   successful test runs - a test can only pass if it explicitly calls
#   STFU_PASS() or STFU_PASS_IFF().
#   
#   Note that no location information is provided in this case.
#   
implicit failure    FAIL - in 0.00115746s

# skipped test: 
#   Demonstration of a test that is skipped because it's disabled. This is
#   useful for temporarily turning off tests without removing them from the
#   test run output.
#   
skipped test        SKIPPED - in 0.00115746s

# slow test: 
#   Demonstration of a test that takes slightly longer to run. Use this to
#   compare the runtime reported for a test vs. those that complete without
#   delay.
#   
slow test           PASS - in 1.00298s

# pass iff: 
#   Demonstration of the STFU_PASS_IFF() macro, which will immediately pass
#   the test if and only if the expression is true. Otherwise, it fails
#   immediately.
#   
pass iff            FAIL (FAILED at test.cc:174) - in 0.00119413s

# assertion: 
#   Demonstration of the case of a failed assertion via STFU_ASSERT(0 == 1).
#   In addition to the filename and line number, it will also include the
#   expression which failed to evaluate to true.
#   
assertion           FAIL (FAILED at test.cc:183: "0 == 1") - in 0.000870209s

# segfault condition: 
#   Demonstration of how an invalid memory access which leads to a
#   segmentation fault will appear as a test failure.
#   
segfault condition  CRASH (crashed with: Segmentation fault) - in 0.000663333s

# Summary: examples completed with 5 failures
% 
```

## Fixtures

Fixtures provide a means of surrounding your test routines with setup/teardown logic which might be required to prepare (and/or clean up) the environment for your tests to run.

It's important to note that fixtures are **not** part of your unit tests, and therefore they do not PASS or FAIL, nor directly cause your test routines to PASS or FAIL, either. Furthermore, they do **not** run in the subprocess context of your unit tests: they do not share the same address space as your test routines, and if your fixture crashes, the entire program will crash.

Fixtures are intended to allow for manipulation of the runtime environment, not for testing you logic. Examples of use cases for fixtures include:
1. Preparing a working directory for tests to run within.
2. Connecting to an external database or server.
3. Uploading test results.

Fixtures are attached to test groups, and can be specified to run once before (or after) _all_ the tests in the group, or once before (or after) _each_ test in the group. Multiple fixtures may be attached, and are run in the order in which they were added.

### Fixture examples

```
stfu::test_group group{"example", "showcase fixtures"};

// Will save and restore the current working directory.
int saved_dir;

group.add_before_all(stfu::test_group::fixture{
                  [&saved_dir]{
                     // Set up a temporary directory to work from.
                     saved_dir = ::open(".", O_SEARCH);
                     return (0 == ::chdir("/tmp/test_directory"));
                  }})
     .add_after_all(stfu::test_group::fixture{
                  [&saved_dir]{
                     // Restore working directory.
                     return (0 == ::fchdir(saved_dir));
                  });
```
