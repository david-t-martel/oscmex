#include "Types.h"
#include <stdexcept>
#include <string>

namespace osc
{

    // Implementation of OSCException class

    OSCException::ErrorCode OSCException::code() const
    {
        return code_;
    }

} // namespace osc
