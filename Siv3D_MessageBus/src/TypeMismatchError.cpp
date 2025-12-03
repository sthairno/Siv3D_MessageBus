#include "MessageBus/TypeMismatchError.hpp"
#include <Siv3D/FormatLiteral.hpp>

using namespace s3d;

namespace MessageBus
{
    TypeMismatchError::TypeMismatchError(StringView key, StringView expected, StringView actual)
        :Error(
            UR"(Type mismatch for variable "{0}". Current setting: {1} Remote type: {2})"_fmt(
                key, expected, actual
            )
        )
    {
    }

    StringView TypeMismatchError::type() const noexcept
    {
        return U"TypeMismatchError";
    }
}
