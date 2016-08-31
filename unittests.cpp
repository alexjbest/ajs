#include <string>
#include <cassert>
#include <cstdarg>
#include "utils.h"

using std::string;
using std::vector;
using std::cout;
using std::endl;

bool test_skip_brackets(const char *t, const size_t last_brace_offset)
{
    string s = t;
    string::iterator end = skip_brackets(s.begin(), s.end());
    if (s.begin() + last_brace_offset != end) {
        cout << "Error, skip_brackets(" << s << ") was wrong\n";
        return false;
    }
    return true;
};

void tests_skip_brackets(void)
{
    assert(test_skip_brackets("", 0));
    assert(test_skip_brackets("a", 0));
    assert(test_skip_brackets("()", 1));
    assert(test_skip_brackets("()a", 1));
    assert(test_skip_brackets("(a)", 2));
    assert(test_skip_brackets("a()", 0));
    assert(test_skip_brackets("()()", 1));
    assert(test_skip_brackets("(())", 3));
    assert(test_skip_brackets("(()())", 5));
    assert(test_skip_brackets("a(())", 0));
    assert(test_skip_brackets("(a())", 4));
    assert(test_skip_brackets("((a))", 4));
    assert(test_skip_brackets("(()a)", 4));
    assert(test_skip_brackets("(())a", 3));
}

bool
test_split_sum(const char *t, const size_t num, ...)
{
    va_list va;
    string s = t;
    vector<string> tokens;
    bool ok = true;

    va_start(va, num);

    tokens = split_sum(s.begin(), s.end());
    if (tokens.size() != num) {
        ok = false;
        cout << "Error, split_sum(" << s << ") returned " << tokens.size()
                << " items, expected " << num << "\n";
    }

    // cout << "Input: " << t << endl;
    for (vector<string>::iterator it = tokens.begin(); ok && it != tokens.end(); it++) {
        const char *expected_token = va_arg (va, const char *);
        if (*it != expected_token) {
            ok = false;
            cout << "Error, got token " << *it << " but expected "
                    << expected_token << endl;
        }
    }

    va_end(va);
    return ok;
}

bool tests_split_sum()
{
    return
    test_split_sum("a", 1, "a") &&
    // I'm not really sure what these cases should return, but we test
    // that they at least return at all
    test_split_sum("+", 1, "+") &&
    test_split_sum("-", 1, "-") &&

    test_split_sum("++", 1, "+") &&
    test_split_sum("+a", 1, "+a") &&
    test_split_sum("a+", 1, "a") &&
    test_split_sum("aa", 1, "aa") &&
    test_split_sum("-a", 1, "-a") &&
    test_split_sum("a-", 2, "a", "-") &&

    test_split_sum("a+b", 2, "a", "b") &&
    test_split_sum("a-b", 2, "a", "-b") &&
    test_split_sum("-a+b", 2, "-a", "b") &&
    test_split_sum("-a-b", 2, "-a", "-b");
}

int main()
{
    tests_skip_brackets();
    tests_split_sum();
    return(0);
}
