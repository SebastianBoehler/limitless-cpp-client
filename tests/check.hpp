#pragma once

#include <cstdlib>
#include <iostream>

namespace limitless::test
{
    inline int &failures()
    {
        static int count = 0;
        return count;
    }

    inline void check(bool condition, const char *expression, const char *file, int line)
    {
        if (!condition)
        {
            std::cerr << file << ":" << line << " failed: " << expression << "\n";
            ++failures();
        }
    }
}

#define CHECK(expression) ::limitless::test::check(static_cast<bool>(expression), #expression, __FILE__, __LINE__)

#define RETURN_TEST() \
    do \
    { \
        if (::limitless::test::failures() != 0) \
        { \
            std::cerr << ::limitless::test::failures() << " check(s) failed\n"; \
            return EXIT_FAILURE; \
        } \
        return EXIT_SUCCESS; \
    } while (0)
