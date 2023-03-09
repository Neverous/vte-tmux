#include "config.h"

#include "parser.hh"

#include "vteinternal.hh"

#include "debug.h"
#include "vtegtk.hh"

namespace vte {

namespace tmux {

void
Parser::set_default_callback (VteTmuxCommandCallback callback,
                              void* data) noexcept
{
        m_default_callback = { callback, data };
}

void
Parser::push_callback (VteTmuxCommandCallback callback, void* data)
{
        m_callbacks.emplace (callback, data);
}

bool
Parser::setup () noexcept
{
        if (m_confirmed)
                return false;

        gboolean requested_confirmation = FALSE;
        g_signal_emit (m_terminal,
                       signals[SIGNAL_TMUX_CONTROL_MODE_ENTER],
                       0,
                       &requested_confirmation);
        return requested_confirmation;
}

bool
Parser::confirm () noexcept
{
        if (m_confirmed)
                return false;

        m_confirmed = true;
        return true;
}

void
Parser::reset () noexcept
{
        g_signal_emit (m_terminal, signals[SIGNAL_TMUX_CONTROL_MODE_EXIT], 0);
        m_confirmed = false;
        m_state = State::GROUND;
        m_command.clear ();
        m_arguments.clear ();
        m_output.clear ();
        m_ignored.clear ();
        m_callbacks = {};
        m_default_callback = { empty_callback, nullptr };
}

std::pair<Parser::Status, uint8_t const*>
Parser::process_incoming (uint8_t const* const bufstart,
                          uint8_t const* const bufend)
{
        auto ip = bufstart;
        while (ip < bufend) {
                auto status = feed (*ip++);
                if (status != Status::CONTINUE) {
                        return { status, ip };
                }
        }

        return { Status::CONTINUE, bufend };
}

Parser::Status
Parser::feed (uint8_t const byte)
{
        switch (m_state) {
        case State::GROUND:
                switch (byte) {
                case '%':
                        return transition (byte, State::COMMAND);

                default:
                        return transition (byte, State::IGNORE);
                }
                break;

        case State::IGNORE:
                switch (byte) {
                case '\n':
                {
                        _vte_debug_print (VTE_DEBUG_PARSER,
                                          "[TMUX] Ignored data \"%s\"\n",
                                          m_ignored.c_str ());

                        std::string message = "ignored data: \"";
                        message += m_ignored;
                        message += "\"";
                        auto [callback, data] = m_default_callback;
                        // FIXMEtmux: fabricate arguments?
                        // timestamp command_no 1
                        callback (false, nullptr, message.c_str (), data);
                        m_ignored.clear ();
                        return transition (byte, State::GROUND);
                }

                default:
                        return consume (byte);
                }
                break;

        case State::COMMAND:
                switch (byte) {
                case '\n':
                        return dispatch (byte);

                case ' ':
                        return transition (byte, State::ARGUMENTS);

                default:
                        return consume (byte);
                }
                break;

        case State::ARGUMENTS:
                switch (byte) {
                case '\n':
                        return dispatch (byte);

                default:
                        return consume (byte);
                }
                break;

        case State::OUTPUT_GROUND:
                switch (byte) {
                case '%':
                        return transition (byte, State::COMMAND);

                default:
                        return transition (byte, State::OUTPUT);
                }
                break;

        case State::OUTPUT:
                switch (byte) {
                case '\n':
                        return transition (byte, State::OUTPUT_GROUND);

                default:
                        return consume (byte);
                }
                break;
        }

        g_assert_not_reached ();
        return Status::ABORT;
}

Parser::Status
Parser::transition (uint8_t const byte, State new_state)
{
        _vte_debug_print (VTE_DEBUG_PARSER,
                          "[TMUX] parser transition %s (%u) -> %s (%u)\n",
                          STATE_TO_STRING[(guint)m_state],
                          (guint)m_state,
                          STATE_TO_STRING[(guint)new_state],
                          (guint)new_state);
        bool do_consume = false;
        switch (new_state) {
        case State::GROUND:
                g_assert (m_state == State::COMMAND ||
                          m_state == State::IGNORE ||
                          m_state == State::ARGUMENTS);
                break;

        case State::IGNORE:
                do_consume = true;
                break;

        case State::COMMAND:
                g_assert (m_state == State::GROUND ||
                          m_state == State::OUTPUT_GROUND);
                do_consume = true;
                break;

        case State::ARGUMENTS:
                g_assert (m_state == State::COMMAND);
                break;

        case State::OUTPUT_GROUND:
                g_assert (m_state == State::COMMAND ||
                          m_state == State::ARGUMENTS ||
                          m_state == State::OUTPUT);
                do_consume = m_state == State::OUTPUT;
                break;

        case State::OUTPUT:
                g_assert (m_state == State::OUTPUT_GROUND);
                do_consume = true;
                break;
        }

        m_state = new_state;
        if (do_consume)
                return consume (byte);

        return Status::CONTINUE;
}

Parser::Status
Parser::consume (uint8_t const byte)
{
        if (byte == '\r')
                return Status::CONTINUE;

        switch (m_state) {
        case State::COMMAND:
                m_command += byte;
                return Status::CONTINUE;

        case State::ARGUMENTS:
                m_arguments += byte;
                return Status::CONTINUE;

        case State::OUTPUT:
        case State::OUTPUT_GROUND:
                m_output += byte;
                return Status::CONTINUE;

        case State::IGNORE:
                m_ignored += byte;
                return Status::CONTINUE;

        case State::GROUND:
                break;
        }

        g_assert_not_reached ();
        return Status::ABORT;
}

Parser::Status
Parser::dispatch (uint8_t const byte)
{
        _vte_debug_print (VTE_DEBUG_PARSER,
                          "[TMUX] Read command %s\n",
                          m_command.c_str ());

        if (m_command == "%begin") {
                m_arguments.clear ();
                return transition (byte, State::OUTPUT_GROUND);
        }

        if (m_command == "%exit") {
                return Status::COMPLETE;
        }

        if (m_command == "%begin%end" || m_command == "%begin%error") {
                _vte_debug_print (VTE_DEBUG_PARSER,
                                  "[TMUX] Read response %s %s: \"%s\"\n",
                                  m_command.c_str (),
                                  m_arguments.c_str (),
                                  m_output.c_str ());

                auto [callback, data] = !m_callbacks.empty ()
                                                ? m_callbacks.front ()
                                                : m_default_callback;
                callback (m_command == "%begin%end",
                          m_arguments.c_str (),
                          m_output.c_str (),
                          data);
                m_command.clear ();
                m_arguments.clear ();
                m_output.clear ();
                if (!m_callbacks.empty ())
                        m_callbacks.pop ();

                return transition (byte, State::GROUND);
        }

        g_assert (m_output.empty ());
        _vte_debug_print (VTE_DEBUG_PARSER,
                          "[TMUX] Emitting command %s %s\n",
                          m_command.c_str (),
                          m_arguments.c_str ());
        g_signal_emit (m_terminal,
                       signals[SIGNAL_TMUX_CONTROL_MODE_COMMAND],
                       0,
                       m_command.c_str (),
                       m_arguments.c_str ());
        m_command.clear ();
        m_arguments.clear ();
        return transition (byte, State::GROUND);
}

} // namespace tmux
} // namespace vte
