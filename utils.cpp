/*
 * utils.cpp
 *
 *  Created on: 29.08.2016
 *      Author: kruppa
 */

#include <utils.h>

std::vector<std::string> split(const std::string &s, char delim, std::vector<std::string> &elems) {
  std::stringstream ss(s);
  std::string item;
  while (std::getline(ss, item, delim)) {
    elems.push_back(item);
  }
  return elems;
}

std::vector<std::string> split(const std::string &s, char delim) {
  std::vector<std::string> elems;
  split(s, delim, elems);
  return elems;
}

std::vector<std::string> split2(const std::string &s, char delim, char delim2, std::vector<std::string> &elems) {
  std::stringstream ss(s);
  std::string item;
  while (std::getline(ss, item, delim)) {
    std::vector<std::string> elems2 = split(item, delim2);
    elems.insert(elems.end(), elems2.begin(), elems2.end());
  }
  return elems;
}

std::vector<std::string> split2(const std::string &s, char delim, char delim2) {
  std::vector<std::string> elems;
  split2(s, delim, delim2, elems);
  return elems;
}

std::string &ltrim(std::string &s) {
  s.erase(s.begin(), std::find_if(s.begin(), s.end(), std::not1(std::ptr_fun<int, int>(std::isspace))));
  return s;
}

std::string &rtrim(std::string &s) {
  s.erase(std::find_if(s.rbegin(), s.rend(), std::not1(std::ptr_fun<int, int>(std::isspace))).base(), s.end());
  return s;
}

std::string &trim(std::string &s) {
  return ltrim(rtrim(s));
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
				std::cout << " " << last;
			else
				std::cout << " " << last << "*" << count;
			if (i < len)
				last = values[i];
			count = 1;
		}
	}
}

template
void print_histogram<int>(const int *values, const size_t len);
