#include <string>
#include <cassert>
#include <cstdarg>
#include <iostream>
#include "utils.h"
#include "eval.h"
#include "ajs_parsing.h"

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

bool
test_eval(const char *expression, eval_type expected_result)
{
    eval_type result = eval(expression);
    if (result != expected_result) {
        cout << "Error evaluating " << expression << ", got result " << result << " but expected " << expected_result << endl;
        return false;
    }
    return true;
}

bool
tests_eval()
{
  return test_eval("1", 1) &&
          test_eval("1+2", 3) &&
          test_eval("1-2", -1) &&
          test_eval("-1+2", 1) &&
          test_eval("-1-2", -3) &&
          test_eval("1*2", 2) &&
          test_eval("1*(2+3)", 5) &&
          test_eval("(1+2)*3", 9) &&
          test_eval("(1+2)*(3+4)", 21) &&
          test_eval("1+(-((2+3)))*(4+5)-6", -50) &&
          test_eval("6/2", 3) &&
          test_eval("1/2", 0) &&
          test_eval("(1+2)*(2+2)/6", 2);
}

bool
test_parse_pointer_intel(const char *ptr_text, uint32_t const size)
{
    asmjit::X86Mem ptr;
    string ptr_string = ptr_text;

    ptr = parse_pointer_intel(ptr_string, size, true);
    return true;
}

bool
tests_parse_pointer_intel()
{
    test_parse_pointer_intel("[123]", 4);
    test_parse_pointer_intel("[rbx]", 4);
    test_parse_pointer_intel("[123+rbx]", 4);
    test_parse_pointer_intel("[rbx*8]", 4);
    test_parse_pointer_intel("[123+rbx*8]", 4);
    test_parse_pointer_intel("[rbx+rsi]", 4);
    test_parse_pointer_intel("[123+rbx+rsi]", 4);
    test_parse_pointer_intel("[123+rbx*8+rsi]", 4);
    test_parse_pointer_intel("[12-4+rbx*8+rsi+(-7*8)]", 4);
    return true;
}


int main()
{
    tests_skip_brackets();
    tests_split_sum();
    tests_eval();
    tests_parse_pointer_intel();
    return(0);
}
