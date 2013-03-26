#include "../include/chrono_io"

namespace std
{
namespace chrono
{

locale::id durationpunct::id;
template <> locale::id timepunct<char>::id;
template <> locale::id timepunct<wchar_t>::id;
template <> locale::id timepunct<char16_t>::id;
template <> locale::id timepunct<char32_t>::id;

}  // chrono

} // std
