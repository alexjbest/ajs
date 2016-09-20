#include <cstddef>
#include <vector>
#include <string>
#include <sstream>
#include <algorithm>
#include <iostream>

#define FPRINTF(__f__, ...) \
    do{ \
        if (fprintf(__f__, __VA_ARGS__) < 0) { \
            perror("fprintf() failed:"); \
            exit(EXIT_FAILURE); \
        } \
    } while (0)

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
    referenceResult(T *r, size_t l) : result(r), len(l)
    {
        if (len > 0) {
            preValue = (T *) malloc(len * sizeof(T));
            correctValue = (T *) malloc(len * sizeof(T));
        }
    }

    ~referenceResult()
    {
        if (len > 0) {
            free(preValue);
            free(correctValue);
        }
    }

    referenceResult(const referenceResult& that) = delete;

    /* Set the data currently in the "result" area as the correct data */
    void setPrevalue() {
        if (len == 0)
            return;
        memcpy (preValue, result, len * sizeof(T));
    }

    /* Set data in the "result" area back to the preValue */
    void resetToPrevalue() const {
        if (len == 0)
            return;
        memcpy(result, preValue, len * sizeof(T));
    }

    /* Set the data currently in the "result" area as the correct data */
    void setCorrect() {
        if (len == 0)
            return;
        memcpy(correctValue, result, len * sizeof(T));
    }

    /* Check whether the data in the "result" area matches the correct data */
    bool check() const {
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
