#pragma once
#include <stdexcept>
#include <string>

namespace gnc {

class GncException : public std::runtime_error {
public:
    GncException(const std::string& component, const std::string& message)
        : std::runtime_error("[" + component + "] " + message) {}
};

class ConfigurationError : public GncException {
public:
    using GncException::GncException;
};

class ValidationError : public GncException {
public:
    using GncException::GncException;
};

class DependencyError : public GncException {
public:
    using GncException::GncException;
};

class StateAccessError : public GncException {
public:
    using GncException::GncException;
};

} // namespace gnc