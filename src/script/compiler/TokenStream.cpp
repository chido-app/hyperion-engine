#include <script/compiler/TokenStream.hpp>

namespace hyperion {
namespace compiler {

TokenStream::TokenStream(const TokenStreamInfo &info)
    : m_position(0),
      m_info(info)
{
}

} // namespace compiler
} // namespace hyperion
