#pragma once

#include <cstdint>
#include <functional>
#include <queue>
#include <string>
#include <utility>

#include "vte/vtetmux.h"

struct _VteTerminal;

namespace vte {

namespace tmux {

inline void
empty_callback (gboolean, const char*, const char*, void*)
{
}

class Parser {
public:
        enum class Status {
                CONTINUE,
                ABORT,
                COMPLETE,
        };

        enum class State {
                GROUND,
                COMMAND,
                ARGUMENTS,
                OUTPUT_GROUND,
                OUTPUT,
                IGNORE,
        };

#ifdef VTE_DEBUG
        static constexpr char const* const STATE_TO_STRING[6] = {
                "GROUND",        "COMMAND", "ARGUMENTS",
                "OUTPUT_GROUND", "OUTPUT",  "IGNORE",
        };
#endif

private:
        VteTerminal* m_terminal;
        bool m_confirmed;
        State m_state;
        std::string m_command;
        std::string m_arguments;
        std::string m_output;
        std::string m_ignored;
        std::pair<VteTmuxCommandCallback, void*> m_default_callback;
        std::queue<decltype (m_default_callback)> m_callbacks;

public:
        Parser (VteTerminal* terminal = nullptr)
                : m_terminal{ terminal },
                  m_confirmed{ false }, m_state{ State::GROUND }, m_command{},
                  m_arguments{}, m_output{}, m_ignored{},
                  m_default_callback{ empty_callback, nullptr },
                  m_callbacks{} {};

        ~Parser () = default;

        Parser (Parser const&) = delete;
        Parser (Parser&&) noexcept = delete;

        Parser& operator= (Parser const&) = delete;
        Parser& operator= (Parser&&) noexcept = delete;

        constexpr bool
        is_confirmed () const noexcept
        {
                return m_confirmed;
        }

        void set_default_callback (VteTmuxCommandCallback callback,
                                   void* data) noexcept;
        void push_callback (VteTmuxCommandCallback callback, void* data);
        bool confirm () noexcept;
        bool setup () noexcept;
        void reset () noexcept;
        std::pair<Status, uint8_t const*>
        process_incoming (uint8_t const* const bufstart,
                          uint8_t const* const bufend);

private:
        Status feed (uint8_t const byte);

        // State manipulation
        Status transition (uint8_t const byte, State new_state);
        Status consume (uint8_t const byte);
        Status dispatch (uint8_t const byte);
}; // class Parser

} // namespace tmux
} // namespace vte
