#pragma once

#if !defined (__VTE_VTE_H_INSIDE__) && !defined (VTE_COMPILATION)
#error "Only <vte/vte.h> can be included directly."
#endif

#include "vteterminal.h"
#include <glib.h>

G_BEGIN_DECLS

typedef void (*VteTmuxCommandCallback)(gboolean, const char*, const char*, void*);


_VTE_PUBLIC
void vte_terminal_set_tmux_parser_default_callback(VteTerminal *terminal, VteTmuxCommandCallback callback, void *data) _VTE_CXX_NOEXCEPT;

_VTE_PUBLIC
void vte_terminal_push_tmux_parser_callback(VteTerminal *terminal, VteTmuxCommandCallback callback, void *data) _VTE_CXX_NOEXCEPT;

/* Set or get whether tmux control mode support is enabled */
_VTE_PUBLIC
void vte_terminal_set_enable_tmux_control_mode(VteTerminal *terminal,
                                               gboolean enabled) _VTE_CXX_NOEXCEPT _VTE_GNUC_NONNULL(1);

_VTE_PUBLIC
gboolean vte_terminal_get_enable_tmux_control_mode(VteTerminal *terminal) _VTE_CXX_NOEXCEPT _VTE_GNUC_NONNULL(1);

_VTE_PUBLIC
void vte_terminal_set_tmux_control_mode_confirmed(VteTerminal *terminal, gboolean confirmed) _VTE_CXX_NOEXCEPT _VTE_GNUC_NONNULL(1);

_VTE_PUBLIC
void vte_terminal_abort_tmux_control_mode(VteTerminal *terminal) _VTE_CXX_NOEXCEPT _VTE_GNUC_NONNULL(1);


G_END_DECLS
