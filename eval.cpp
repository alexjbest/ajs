/*
 * my_eval.cpp
 *
 *  Created on: 01.09.2016
 *      Author: kruppa
 */


/*
 * expr        := summand (add_op summand)*
 * summand     := factor (mult_op factor)*
 * factor      := sign ( expr ) | sign number
 * add_op      := + | -
 * mult_op     := * | /
 * sign        := + | - | e
 */

#include <iostream>
#include <cstdlib>
#include "eval.h"

using std::cout;
using std::endl;

static eval_type parse_expr(const char * &expr);

static bool
consume(const char * &expr, const char c)
{
    while (expr[0] == ' ')
        expr++;
    bool match = (expr[0] == c);
    expr += match ? 1 : 0;
    return match;
}

static bool
consume_or_die(const char * &expr, const char c)
{
    if (!consume(expr, c)) {
        cout << "Error: got character " << expr[0] << ", expected " << c << endl;
        exit(EXIT_FAILURE);
    }
    return true;
}

static int
parse_sign(const char * &expr)
{
    if (consume(expr, '-'))
        return -1;
    consume(expr, '+');
    return 1;
}

static eval_type
parse_factor(const char * &expr)
{
    eval_type result;
    const int sign = parse_sign(expr);

    if (consume(expr, '(')) {
        result = parse_expr(expr);
        consume_or_die(expr, ')');
    } else {
        char *end_expr;
        result = strtoul(expr, &end_expr, 10);
        expr = end_expr;
    }

    return sign*result;
}

static eval_type
parse_summand(const char * &expr)
{
    eval_type result = parse_factor(expr);
    while(1) {
        if (consume(expr, '*')) {
            result *= parse_factor(expr);
        } else if (consume(expr, '/')) {
            result /= parse_factor(expr);
        } else
            break;
    }
    return result;
}

static eval_type
parse_expr(const char * &expr)
{
    eval_type result = parse_summand(expr);
    while(1) {
        if (consume(expr, '+')) {
            result += parse_summand(expr);
        } else if (consume(expr, '-')) {
            result -= parse_summand(expr);
        } else
            break;
    }
    return result;
}

eval_type
eval(const char * expr)
{
    return parse_expr(expr);
}
