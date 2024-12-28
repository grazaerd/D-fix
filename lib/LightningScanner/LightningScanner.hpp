#pragma once
#include <cstdint>
#include <vector>

#include "../LightningScanner/Pattern.hpp"
#include "../LightningScanner/ScanMode.hpp"
#include "../LightningScanner/ScanResult.hpp"

#include "../LightningScanner/CpuInfo.hpp"
#include "../LightningScanner/backends/Avx2.hpp"
#include "../LightningScanner/backends/Scalar.hpp"
#include "../LightningScanner/backends/Sse42.hpp"

namespace LightningScanner {

/**
 * \brief Single result IDA-style pattern scanner
 * \headerfile LightningScanner.hpp <LightningScanner/LightningScanner.hpp>
 *
 * A pattern scanner that searches for an IDA-style pattern
 * and returns the pointer to the first occurrence in the binary.
 *
 * \tparam PreferredMode the preferred scan mode.
 *          This is used as a suggestion for the scanner which mode to try
 * first, if the mode is not supported, it will try every other one until it
 * finds a supported one.
 */
template <ScanMode PreferredMode = ScanMode::Scalar>
class Scanner {
public:
    /**
     * Create a new Scanner instance with an IDA-style pattern
     *
     * Example:
     *
     * \code{.cpp}
     * LightningScanner::Scanner("48 89 5c 24 ?? 48 89 6c");
     * \endcode
     */
    Scanner(Pattern pattern) : m_Pattern(pattern) {}

    /**
     * Find the first occurrence of the pattern in the binary
     *
     * \param{in} startAddr address to start the search from
     * \param{in} size binary size of the search area
     *
     * \return A ScanResult instance
     *
     * Example:
     *
     * \code{.cpp}
     * using namespace LightningScanner;
     * const auto scanner = Scanner("48 89 5c 24 ?? 48 89 6c");
     * ScanResult result = scanner.Find(binary, binarySize);
     * \endcode
     */
    ScanResult Find(void* startAddr, size_t size) const {
        return FindScalar(m_Pattern, startAddr, size);
    }

private:
    Pattern m_Pattern;
};

} // namespace LightningScanner