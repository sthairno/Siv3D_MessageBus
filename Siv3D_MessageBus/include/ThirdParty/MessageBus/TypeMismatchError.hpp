#pragma once
#include <Siv3D/Error.hpp>

namespace MessageBus
{
    /// @brief 型が一致していないときのエラー
    class TypeMismatchError final : public s3d::Error
    {
    public:
        using Error::Error;

        TypeMismatchError(s3d::StringView key, s3d::StringView expected, s3d::StringView actual);

        [[nodiscard]]
        s3d::StringView type() const noexcept override;
    };
}
