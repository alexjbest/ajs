/*
 * ajs_parsing.cpp
 *
 *  Created on: 01.09.2016
 *      Author: kruppa
 */

#include <cassert>
#include <vector>
#include <iostream>
#include <algorithm>
#include "utils.h"
#include "eval.h"
#include "ajs_parsing.h"

using std::string;
using std::vector;
using std::cout;
using std::endl;
using asmjit::X86Mem;
using asmjit::X86GpReg;
using asmjit::x86::noGpReg;
using asmjit::x86::ptr_abs;
using asmjit::x86::ptr;

X86Mem
parse_pointer_intel(string addr, uint32_t const size, bool const verbose)
{
    assert (std::count(addr.begin(), addr.end(), '[') == 1);
    assert (std::count(addr.begin(), addr.end(), ']') == 1);

    size_t start = addr.find('['),
           end = addr.find(']');
    assert (start != string::npos && end != string::npos);
    assert (start + 1 < end);
    // addr2 is only the text between the "[" and "]" brackets
    string addr2 = addr.substr(start + 1, end - start - 1);

    vector<string> tokens = split_sum(addr2.begin(), addr2.end());
    X86GpReg base = noGpReg, index = noGpReg;
    uint32_t index_shift = 0;
    int32_t disp = 0;

    for (vector<string>::iterator it = tokens.begin(); it != tokens.end(); it++) {
        if (containsAlpha(*it)) {
            int shift = 0;
            X86GpReg reg = noGpReg;
            vector<string> words = split(*it, '*');
            for (vector<string>::iterator it2 = words.begin(); it2 != words.end(); it2++){
                if (containsAlpha(*it2)){
                    assert(reg == noGpReg);
                    reg = getGpRegFromName(*it2);
                    assert(reg != noGpReg);
                } else {
                    assert(shift == 0);
                    switch (it2->at(0)) {
                        case '1' : shift = 0; break;
                        case '2' : shift = 1; break;
                        case '4' : shift = 2; break;
                        case '8' : shift = 3; break;
                        default: abort();
                    }
                }
            }
            if (shift == 0 && base == noGpReg) {
                base = reg;
            } else if (index == noGpReg) {
                index = reg;
                index_shift = shift;
            } else {
                cout << "Error parsing pointer: " << addr2 << endl;
                abort();
            }
        } else { /* if (containsAlpha(*it)) */
            int32_t value = eval(it->c_str());
            if (verbose) {
                cout << "Evaluating " << *it << " produced " << value << endl;
            }
            disp += value;
        }
    }

    if (index != noGpReg) {
        if (base == noGpReg) {
            if (verbose) {
                cout << "Parsing " << addr << " produced ptr_abs(base=0, index=" << index.getRegCode()
                        << ", shift=" << index_shift << ", disp=" << disp << ", size=" << size << ")" << endl;
            }
            return ptr_abs(0, index, index_shift, disp, size);
        }
        if (verbose) {
            cout << "Parsing " << addr << " produced ptr(base=" << base.getRegCode() << ", index=" << index.getRegCode()
                    << ", shift=" << index_shift << ", disp=" << disp << ", size=" << size << ")" << endl;
        }
        return ptr(base, index, index_shift, disp, size);
    }
    if (base == noGpReg) {
        if (verbose) {
            cout << "Parsing " << addr << " produced ptr_abs(base=0, disp=" << disp << ", size=" << size << ")" << endl;
        }
        return ptr_abs(0, disp, size);
    }
    if (verbose) {
        cout << "Parsing " << addr << " produced ptr(base=" << base.getRegCode() << ", disp=" << disp << ", size=" << size << ")" << endl;
    }
    return ptr(base, disp, size);
}
