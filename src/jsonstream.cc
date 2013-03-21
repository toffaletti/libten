#include "ten/jsonstream.hh"
#include "ten/logging.hh"
#include "double-conversion.h"

namespace ten {

std::string jsonstream::double_to_string(double val) {
    using ::double_conversion::DoubleToStringConverter;
    using ::double_conversion::StringBuilder;
    constexpr char infinity_symbol[] = "null";
    constexpr char nan_symbol[] = "null";
    constexpr char exponent_symbol = 'e';

    DoubleToStringConverter conv(
            DoubleToStringConverter::EMIT_POSITIVE_EXPONENT_SIGN |
                DoubleToStringConverter::EMIT_TRAILING_DECIMAL_POINT | 
                DoubleToStringConverter::EMIT_TRAILING_ZERO_AFTER_POINT | 
                DoubleToStringConverter::UNIQUE_ZERO,
            infinity_symbol,
            nan_symbol,
            exponent_symbol,
            -6, 21,
            0, 0);

    char buf[128];
    StringBuilder builder(buf, sizeof(buf));
    conv.ToShortest(val, &builder);
    DCHECK_GT(builder.position(), 0);
    builder.Finalize();
    return buf;
}

} // ten

