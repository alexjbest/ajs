#include <cstddef>
#include <vector>
#include <string>
#include <sstream>
#include <algorithm>
#include <iostream>
#include <cassert>

#define FPRINTF(__f__, ...) \
    do{ \
        if (fprintf(__f__, __VA_ARGS__) < 0) { \
            perror("fprintf() failed:"); \
            exit(EXIT_FAILURE); \
        } \
    } while (0)

static inline void
ularith_div_2ul_ul_ul (unsigned long *q, unsigned long *r, 
                       const unsigned long a1, const unsigned long a2, 
                       const unsigned long b)
{
  assert(a2 < b); /* Or there will be quotient overflow */
  __asm__ (
    "divq %4"
    : "=a" (*q), "=d" (*r)
    : "0" (a1), "1" (a2), "rm" (b)
    : "cc");
}

template <typename T>
T *ptr_add_bytes(T *p, size_t n) {
    assert(n % sizeof(T) == 0);
    return p + n / sizeof(T);
}

/* Functor that tests whether a value is in [start, end[.
 * Note that the end value is NOT included in the range, similarly
 * as in Python. */
template <typename T>
class is_in_range
{
    public:
        is_in_range (T start, T end) : _start( start ), _end( end ) {}
        bool operator() (T y) const { return y >= _start && y < _end; }
    private:
        T _start, _end;
};

/* Checks whether data in a "result" memory area of length "len"
 * (in units of elements of type T) matches a previous value. */
template <typename T>
class referenceResult {
private:
    T * const result; /* Points to the memory area that will be checked */
    const size_t len; /* Number of elements in the "result" area */
    T *correctValue, *preValue;

public:
    referenceResult(T *r, size_t l) : result(r), len(l), correctValue(NULL), preValue(NULL)
    {}

    ~referenceResult()
    {
        if (preValue != NULL) {
            free(preValue);
        }
        if (correctValue != NULL) {
            free(correctValue);
        }
    }

    referenceResult(const referenceResult& that) = delete;

    /* Set the data currently in the "result" area as the input data */
    void setPrevalue() {
        if (len == 0)
            return;
        if (preValue == NULL) {
            preValue = (T *) malloc(len * sizeof(T));
        }
        memcpy (preValue, result, len * sizeof(T));
    }

    /* Return whether a prevalue has been set or not */
    bool havePrevalue() const {
        return (preValue != NULL);
    }

    /* Set data in the "result" area back to the prevalue */
    void resetToPrevalue() const {
        if (len == 0)
            return;
        assert(havePrevalue());
        memcpy(result, preValue, len * sizeof(T));
    }

    /* Set the data currently in the "result" area as the correct output data */
    void setCorrect() {
        if (len == 0)
            return;
        if (correctValue == NULL) {
            correctValue = (T *) malloc(len * sizeof(T));
        }
        memcpy(correctValue, result, len * sizeof(T));
    }

    /* Return whether a correct value has been set or not */
    bool haveCorrectValue() const {
        return (correctValue != NULL);
    }

    /* Check whether the data in the "result" area matches the correct data */
    bool check() const {
        assert(haveCorrectValue());
        if (len == 0 || memcmp(correctValue, result, len * sizeof(T)) == 0) {
            return true;
        }
        fprintf(stderr, "Error, data does not match reference value\n");
        abort();
    }
};


std::vector<std::string> split(const std::string &s, char delim, std::vector<std::string> &elems);

std::vector<std::string> split(const std::string &s, char delim);

std::vector<std::string> split2(const std::string &s, char delim, char delim2, std::vector<std::string> &elems);

std::vector<std::string> split2(const std::string &s, char delim, char delim2);

// trim from start
std::string &ltrim(std::string &s);

// trim from end
std::string &rtrim(std::string &s);

// trim from both ends
std::string &trim(std::string &s);

bool containsAlpha(const std::string &s);

template <typename element_type>
void print_histogram(const element_type *values, const size_t len);

std::string::iterator
skip_brackets(std::string::iterator start, std::string::iterator end);

std::vector<std::string>
split_sum(std::string::iterator start, std::string::iterator end);

int64_t
getVal(std::string val);

char *
readLine(char * &lineBuf, size_t &lineBufSize, FILE *inputFile);

long int
get_nivcsw();
