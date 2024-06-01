//
// Copyright (c) 2024 Bryan Phillippe
//
// This software is free to use for any purpose, provided this copyright
// notice is preserved.
//

#pragma once

#include <iostream>
#include <iomanip>
#include <streambuf>
#include <string>
#include <functional>
#include <chrono>
#include <utility>
#include <vector>
#include <stdexcept>

#include <unistd.h>
#include <csignal>
#include <sys/wait.h>

//
// Conclude the test routine with a passing result.
//
#define STFU_PASS()     do \
    { throw stfu_private::pass{}; } \
    while (0)

//
// If and only if the condition evaluates to true, then conclude the test
// routine with a passing result. Else, conclude the test routine with a
// failing result.
//
#define STFU_PASS_IFF(x) do \
    { if ((x)) STFU_PASS(); else STFU_FAIL(); } \
    while (0)

//
// Conclude the test routine with a failing result.
//
#define STFU_FAIL()     do \
    { throw stfu_private::failed_at{__FILE__, __LINE__}; } \
    while (0)

//
// If the condition does not evaluate true, then conclude the test routine
// with a failing result. Otherwise, continue the test routine.
//
#define STFU_ASSERT(x)  do \
    { if (!(x)) throw stfu_private::failed_assert{__FILE__, __LINE__, #x}; } \
    while (0)

namespace stfu {

    //
    // Various states that a test execution can yield.
    //

    enum class test_result {
        DIDNT_RUN,
        SKIPPED,
        PASS,
        FAIL,
        CRASH
    };

    //
    // Test runs may contain metadata regarding the test execution.
    //

    struct test_result_data {
        test_result result{test_result::DIDNT_RUN};
        std::string message{};
        std::chrono::duration<double> runtime{};
    };

    //
    // Encapsulation of a test routine.
    //

    class test {
        public:

        using test_routine = std::function<void()>;

        test(const char* name,
             test_routine routine,
             const char* description = "") noexcept;
        ~test();

        const std::string& get_name() const noexcept;
        const std::string& get_description() const noexcept;
        bool is_enabled() const noexcept;

        test& set_enable(bool) noexcept;

        test_result_data operator()() const;

        protected:

        test_routine fn;

        mutable int filedes[2] = { -1, -1 };
        enum pipe_end {
            read_end = 0,
            write_end = 1
        };

        std::string name;
        std::string description;
        bool enabled = true;

        void write_result(test_result) const noexcept;
        void write_result(test_result, const std::string&) const noexcept;
        test_result_data read_result() const noexcept;
        void close_handle(pipe_end) const noexcept;
    };

    //
    // Group of related tests.
    //

    class test_group {
        public:

        using fixture = std::function<bool()>;

        explicit test_group(const char* name,
                            const char* description = "") noexcept;

        test_group& set_verbose(bool) noexcept;
        test_group& add_test(const stfu::test&);

        test_group& add_before_all(const fixture&);
        test_group& add_before_each(const fixture&);
        test_group& add_after_all(const fixture&);
        test_group& add_after_each(const fixture&);

        size_t operator()(std::ostream& = std::cout) const;

        protected:

        std::vector<fixture> before_all;
        std::vector<fixture> before_each;
        std::vector<fixture> after_all;
        std::vector<fixture> after_each;

        std::vector<stfu::test> tests;

        std::string name;
        std::string description;
        bool verbose = true;
    };
}

namespace stfu_private {

    //
    // A class used to denote a passing test. This is done to ensure that
    // tests can never pass implicitly; all passing results must be denoted
    // explicitly by throwing a class of this type (as opposed to returning
    // true or zero or something that might happen accidentally in the
    // course of the test routine).
    //

    class pass {};

    //
    // The base type for all failure results. Can not be instantiated
    // outside a more specific type of failure.
    //

    class fail {
        public:

        const std::string& get_message() const noexcept;

        protected:

        std::string message;

        fail() noexcept;
    };

    //
    // A specific failure type, indicating a failure which occurred at a
    // location in the code denoted by a filename and line number.
    //

    class failed_at: public fail {
        public:

        explicit failed_at(const char*, size_t) noexcept;
    };

    //
    // A specific failure type, indicating a failure which occurred at a
    // location in the code denoted by a filename and line number, and an
    // expression which did not evaluate to true (e.g. failed assertion).
    //

    class failed_assert: public failed_at {
        public:

        explicit failed_assert(const char*, size_t, const char*) noexcept;
    };

    //
    // A class used to handle failures in fixtures supporting test routines.
    //

    class fixture_exception: public std::runtime_error {
        public:

        explicit fixture_exception(const char *);
    };

    class widthbuf: public std::streambuf {
        public:

        widthbuf(size_t w, std::streambuf* s);

        protected:

        typedef std::basic_string<char_type> string;

        static const size_t tab_width = 8;
        const std::string prefix = "#   ";

        size_t width;
        size_t count;

        std::streambuf* sbuf;
        string buffer;

        int_type overflow(int_type c) override;
    };

    class widthstream: public std::ostream {
        public:

        widthstream(size_t width, std::ostream &os);

        protected:

        widthbuf buf;
    };
}

//
// Pretty-print the output of a test result.
//

inline std::ostream&
operator<<(std::ostream& out, const stfu::test_result_data& d)
{
    switch (d.result) {
    case stfu::test_result::DIDNT_RUN:
        out << "\aDIDNT_RUN";
        break;

    case stfu::test_result::SKIPPED:
        out << "SKIPPED";
        break;

    case stfu::test_result::PASS:
        out << "PASS";
        break;

    case stfu::test_result::FAIL:
        out << "\aFAIL";
        break;

    case stfu::test_result::CRASH:
        out << "\aCRASH";
        break;
    }

    if (!d.message.empty()) {
        out << " (" << d.message << ")";
    }

    return out;
}

//
// test class implementation
//

inline
stfu::test::test(const char* n, test_routine f, const char* d) noexcept:
    fn{std::move(f)}, name{n}, description{d}
{
}

inline
stfu::test::~test()
{
    for (int i: filedes) {
        if (-1 != i) {
            ::close(i);
        }
    }
}

inline const std::string&
stfu::test::get_name() const noexcept
{
    return name;
}

inline const std::string&
stfu::test::get_description() const noexcept
{
    return description;
}

inline bool
stfu::test::is_enabled() const noexcept
{
    return enabled;
}

inline stfu::test&
stfu::test::set_enable(bool b) noexcept
{
    enabled = b;
    return *this;
}

inline void
stfu::test::write_result(test_result r) const noexcept
{
    write_result(r, "");
}

inline void
stfu::test::write_result(test_result r, const std::string& m) const noexcept
{
    switch (r) {
    case test_result::PASS:
        ::write(filedes[write_end], "PASS", 4);
        break;

    default:
        ::write(filedes[write_end], "FAIL", 4);
    }

    if (!m.empty()) {
        ::write(filedes[write_end], m.c_str(), m.length());
    }

    close_handle(write_end);
}

inline stfu::test_result_data
stfu::test::read_result() const noexcept
{
    test_result_data r;
    char buffer[128];
    ssize_t l;
    std::string message;

    l = ::read(filedes[read_end], buffer, sizeof(buffer));

    if (l < 1) {
        message = "Test system failure";
    } else {
        message.assign(buffer, l);
    }

    if (0 == message.compare(0, 4, "PASS")) {
        r.result = test_result::PASS;
    } else {
        r.result = test_result::FAIL;
    }

    if (4 < l) {
        r.message = message.substr(4);
    }

    return r;
}

inline void
stfu::test::close_handle(pipe_end e) const noexcept
{
    ::close(filedes[e]);
    filedes[e] = -1;
}

inline stfu::test_result_data
stfu::test::operator()() const
{
    using namespace std::chrono;

    stfu::test_result_data r;

    if (!enabled) {
        r.result = stfu::test_result::SKIPPED;
        return r;
    }

    if (0 != ::pipe(filedes)) {
        return r;
    }

    auto t1 = high_resolution_clock::now();

    switch (pid_t pid = ::fork()) {
    // Error case
    case -1:
        break;

    // Child
    case 0:
        close_handle(read_end);

        try {
            fn();
        } catch (const stfu_private::pass&) {
            write_result(test_result::PASS);
            ::exit(0);
        } catch (const stfu_private::fail& e) {
            write_result(test_result::FAIL, e.get_message());
            ::exit(0);
        }
        ::exit(-1);

    // Parent
    default:
        close_handle(write_end);

        int stat_loc;

        // Test ran; default result is FAIL.
        r.result = stfu::test_result::FAIL;

        // Unable to reap the child.
        if (::waitpid(pid, &stat_loc, 0) != pid) {
            break;
        }

        // Iff the child exited with 0, read the test result.
        if (WIFEXITED(stat_loc) && (0 == WEXITSTATUS(stat_loc))) {
            r = read_result();
            break;
        }

        // Any signal-termination condition is considered a CRASH.
        if (WIFSIGNALED(stat_loc)) {
            r.result = stfu::test_result::CRASH;

            const int signal = WTERMSIG(stat_loc);
            if (signal < NSIG) {
                r.message.append("crashed with: ")
                         .append(::sys_siglist[signal]);
            }
            break;
        }
    }

    auto t2 = high_resolution_clock::now();

    r.runtime = duration_cast<duration<double>>(t2 - t1);

    return r;
}

//
// test_group implementation
//

inline
stfu::test_group::test_group(const char* n, const char* d) noexcept:
    name{n}, description{d}
{
}

inline stfu::test_group&
stfu::test_group::set_verbose(bool b) noexcept
{
    verbose = b;
    return *this;
}

inline stfu::test_group&
stfu::test_group::add_test(const stfu::test& test)
{
    tests.push_back(test);
    return *this;
}

inline stfu::test_group&
stfu::test_group::add_before_all(const fixture& f)
{
    before_all.push_back(f);
    return *this;
}

inline stfu::test_group&
stfu::test_group::add_before_each(const fixture& f)
{
    before_each.push_back(f);
    return *this;
}

inline stfu::test_group&
stfu::test_group::add_after_all(const fixture& f)
{
    after_all.push_back(f);
    return *this;
}

inline stfu::test_group&
stfu::test_group::add_after_each(const fixture& f)
{
    after_each.push_back(f);
    return *this;
}

inline size_t
stfu::test_group::operator()(std::ostream& out) const
{
    using stfu_private::fixture_exception;

    stfu_private::widthstream wrapped_comment{75, out};
    size_t failures = 0;

    if (verbose) {
        out << "#" << std::endl
            << "# Running " << tests.size() << " test(s) "
            << "in group: " << name << std::endl;

        out << "#" << std::endl
            << "# " << description << std::endl
            << "#" << std::endl;
    }

    try {

        // Run global prefixes.
        for (const auto &f: before_all) {
            if (!f()) {
                throw fixture_exception("before_all");
            }
        }

        // Run all tests.
        for (const auto &t: tests) {

            // Run per-test prefixes.
            for (const auto &f: before_each) {
                if (!f()) {
                    throw fixture_exception("before_each");
                }
            }

            // Run the test routine.
            const auto r = t();

            // Run per-test postfixes.
            for (const auto &f: after_each) {
                if (!f()) {
                    throw fixture_exception("after_each");
                }
            }

            if (verbose) {
                out << "# " << t.get_name() << ": " << std::endl;
                wrapped_comment << t.get_description() << std::endl
                                << std::endl;
            }

            if ((stfu::test_result::PASS != r.result) &&
                (stfu::test_result::SKIPPED != r.result)) {
                ++failures;
            }

            out << std::setw(20) << std::left << t.get_name()
                << r
                << " - in " << r.runtime.count() << "s"
                << std::endl;

            if (verbose) {
                out << std::endl;
            }
        }

        // Run global postfixes.
        for (const auto &f: after_all) {
            if (!f()) {
                throw fixture_exception("after_all");
            }
        }
    }

    catch (stfu_private::fixture_exception& e) {
        out << "# ERROR - failure in fixture: " << e.what() << std::endl;
    }

    if (verbose) {
        out << "# Summary: " << name << " completed with " << failures
            << ((1 == failures) ? " failure" : " failures") << std::endl;
    }

    return failures;
}

inline const std::string&
stfu_private::fail::get_message() const noexcept
{
    return message;
}

inline
stfu_private::fail::fail() noexcept:
    message{"FAILED"}
{
}

inline
stfu_private::failed_at::failed_at(const char* f, size_t l) noexcept
{
    message.append(" at ")
           .append(f)
           .append(":")
           .append(std::to_string(l));
}

inline
stfu_private::failed_assert::failed_assert(const char* f, size_t l,
        const char* x) noexcept:
    failed_at{f, l}
{
    message.append(": \"")
           .append(x)
           .append("\"");
}

inline
stfu_private::fixture_exception::fixture_exception(const char *m):
    std::runtime_error{m}
{
}

inline
stfu_private::widthbuf::widthbuf(size_t w, std::streambuf* s):
    width{w}, count{0}, sbuf{s}
{
}

//
// This is basically a line-buffering stream buffer.
// The algorithm is:
// - Explicit end of line ("\r" or "\n"): we flush our buffer
//   to the underlying stream's buffer, and set our record of
//   the line length to 0.
// - An "alert" character: sent to the underlying stream
//   without recording its length, since it doesn't normally
//   affect the appearance of the output.
// - tab: treated as moving to the next tab stop, which is
//   assumed as happening every tab_width characters.
// - Everything else: really basic buffering with word wrapping.
//   We try to add the character to the buffer, and if it exceeds
//   our line width, we search for the last space/tab in the
//   buffer and break the line there. If there is no space/tab,
//   we break the line at the limit.
//
inline stfu_private::widthbuf::int_type
stfu_private::widthbuf::overflow(int_type c)
{
    if (traits_type::eq_int_type(traits_type::eof(), c)) {
        return traits_type::not_eof(c);
    }

    switch (c) {
    case '\n':
    case '\r': {
        buffer += static_cast<char>(c);
        count = 0;
        sbuf->sputn(prefix.c_str(), prefix.length());
        int_type rc = sbuf->sputn(buffer.c_str(), buffer.size());
        buffer.clear();
        return rc;
    }

    case '\a':
        return sbuf->sputc(static_cast<char>(c));

    case '\t':
        buffer += static_cast<char>(c);
        count += tab_width - count % tab_width;
        return c;

    default:
        if (count >= width) {
            size_t wpos = buffer.find_last_of(" \t");
            if (wpos != string::npos) {
                sbuf->sputn(prefix.c_str(), prefix.length());
                sbuf->sputn(buffer.c_str(), wpos);
                count = buffer.size()-wpos-1;
                buffer = string(buffer, wpos+1);
            } else {
                sbuf->sputn(prefix.c_str(), prefix.length());
                sbuf->sputn(buffer.c_str(), buffer.size());
                buffer.clear();
                count = 0;
            }
            sbuf->sputc('\n');
        }
        buffer += static_cast<char>(c);
        ++count;
        return c;
    }
}

inline
stfu_private::widthstream::widthstream(size_t width, std::ostream &os):
    std::ostream{&buf}, buf{width, os.rdbuf()}
{
}
