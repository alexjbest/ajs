/*
 * utils.cpp
 *
 *  Created on: 29.08.2016
 *      Author: kruppa
 */

#include <cctype>
#include <cassert>
#include "utils.h"

using std::endl;
using std::cout;
using std::string;
using std::vector;

vector<string> split(const string &s, char delim, vector<string> &elems) {
  std::stringstream ss(s);
  string item;
  while (std::getline(ss, item, delim)) {
    elems.push_back(item);
  }
  return elems;
}

vector<string> split(const string &s, char delim) {
  vector<string> elems;
  split(s, delim, elems);
  return elems;
}

vector<string> split2(const string &s, char delim, char delim2, vector<string> &elems) {
  std::stringstream ss(s);
  string item;
  while (std::getline(ss, item, delim)) {
    vector<string> elems2 = split(item, delim2);
    elems.insert(elems.end(), elems2.begin(), elems2.end());
  }
  return elems;
}

vector<string> split2(const string &s, char delim, char delim2) {
  vector<string> elems;
  split2(s, delim, delim2, elems);
  return elems;
}

string &ltrim(string &s) {
  s.erase(s.begin(), std::find_if(s.begin(), s.end(), std::not1(std::ptr_fun<int, int>(std::isspace))));
  return s;
}

string &rtrim(string &s) {
  s.erase(std::find_if(s.rbegin(), s.rend(), std::not1(std::ptr_fun<int, int>(std::isspace))).base(), s.end());
  return s;
}

string &trim(string &s) {
  return ltrim(rtrim(s));
}

bool containsAlpha(const string &s)
{
    for (string::const_iterator it = s.begin(); it != s.end(); it++) {
        if (isalpha(*it))
            return true;
    }
    return false;
}

template <typename element_type>
void print_histogram(const element_type *values, const size_t len)
{
	size_t count = 1;

	if (len == 0)
		return;

	element_type last = values[0];
	for (int i = 1; i <= len; i++) {
		if (i < len && values[i] == last) {
			count++;
		} else {
			if (count == 1)
				cout << " " << last;
			else
				cout << " " << last << "*" << count;
			if (i < len)
				last = values[i];
			count = 1;
		}
	}
}

template
void print_histogram<int>(const int *values, const size_t len);

string::iterator
skip_brackets(string::iterator start, string::iterator end)
{
    string::iterator it = start;

    /* For empty input string, or if first character is not an open bracket,
     * return start iterator. If the first character is an open bracket,
     * return an iterator pointing to the matching closing bracket.
     * If there is no matching closing bracket, return end iterator. */
    if (it == end)
        return it;
    if (*it != '(')
        return it;

    size_t nr_open_brackets = 1;
    it++;
    while (nr_open_brackets > 0 && it != end) {
        if (*it == '(') nr_open_brackets++;
        if (*it == ')') nr_open_brackets--;
        if (nr_open_brackets > 0)
            it++;
    }
    return it;
}

vector<string>
split_sum(string::iterator start, string::iterator end)
{
    string::iterator token_start;
    bool had_text = false;
    vector<string> tokens;

    for (string::iterator it = start; it != end; it++) {
        if (!had_text)
            token_start = it;
        /* Skip brackets */
        const string::iterator it2 = skip_brackets(it, end);
        if (it2 != it) {
            assert (it2 != end); /* Make sure brackets were balanced */
            had_text = true;
            it = it2;
        } else if ((*it == '+' || *it == '-') && !had_text) {
            // Consume any sign
            had_text = true;
        } else if (*it == '+') { // Implies had_text == true
            // cout << "Pushing token " << string(token_start, it) << endl;
            tokens.push_back(string(token_start, it));
            had_text = false;
        } else if (*it == '-') { // Implies had_text == true
            // This case occurs for example in "[A-B]". Here we push the
            // token "A" and let the new token begin at "-", so that the
            // second token will become "-B" and the "-" sign takes a
            // unary role.
            // cout << "Pushing token " << string(token_start, it) << endl;
            tokens.push_back(string(token_start, it));
            token_start = it;
            had_text = true;
        } else {
            had_text = true;
        }
    }
    if (had_text) {
        // cout << "Pushing token " << string(token_start, end) << endl;
        tokens.push_back(string(token_start, end));
    }

    return tokens;
}
