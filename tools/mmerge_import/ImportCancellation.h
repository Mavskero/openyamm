#pragma once

#include <exception>
#include <functional>

namespace OpenYAMM::Import
{
using ImportCancelCheck = std::function<bool()>;

class ImportCancelled : public std::exception
{
public:
    const char *what() const noexcept override { return "Resource import cancelled"; }
};

inline void checkImportCancellation(const ImportCancelCheck &cancelled)
{
    if (cancelled && cancelled())
    {
        throw ImportCancelled();
    }
}
}
