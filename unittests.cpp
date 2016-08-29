#include <string>
#include <cassert>
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

void
test_split_sum(const char *t)
{
    string s = t;
    vector<string> tokens;
    tokens = split_sum(s.begin(), s.end());
    cout << "Input: " << t << endl;
    for (vector<string>::iterator it = tokens.begin(); it != tokens.end(); it++)
        cout << "Token: " << *it << endl;
}

void tests_split_sum()
{
    test_split_sum("a");
    test_split_sum("+");
    test_split_sum("-");

    test_split_sum("++");
    test_split_sum("+a");
    test_split_sum("a+");
    test_split_sum("aa");
    test_split_sum("-a");
    test_split_sum("a-");

    test_split_sum("a+b");
    test_split_sum("a-b");
    test_split_sum("-a+b");
    test_split_sum("-a-b");
}

int main()
{
    tests_skip_brackets();
    tests_split_sum();
    return(0);
}
