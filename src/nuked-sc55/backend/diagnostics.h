#pragma once

#include <string_view>

enum class Diag_Category
{
    Debug,
    Error,
    Warning,
};

const char* ToCString(Diag_Category category);

using Diag_Callback = void(*)(Diag_Category category, std::string_view message);

// The default callback handler which prints messages to `stderr`.
void Diag_DefaultCallback(Diag_Category category, std::string_view message);

// Sends a message from the backend to the callback.
void Diag_Printf(Diag_Category category, const char* format, ...);

// Sets the callback. This should be called before creating any emulators.
// Callbacks can run on any thread that calls functions in the backend. If
// there are multiple threads making such calls, the callback may need
// synchronization.
//
// Passing `nullptr` will disable logging entirely.
void Diag_SetCallback(Diag_Callback callback);
