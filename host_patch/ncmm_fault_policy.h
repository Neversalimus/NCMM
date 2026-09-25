#pragma once

namespace ncmm
{
enum class runtime_callback_kind {
    turn,
    locale,
    ui
};

struct runtime_fault_policy {
    bool turn_quarantined = false;
    bool locale_quarantined = false;
    bool ui_quarantined = false;
    bool modifiers_quarantined = false;

    bool quarantine( runtime_callback_kind kind )
    {
        bool *target = nullptr;
        switch( kind ) {
            case runtime_callback_kind::turn:
                target = &turn_quarantined;
                break;
            case runtime_callback_kind::locale:
                target = &locale_quarantined;
                break;
            case runtime_callback_kind::ui:
                target = &ui_quarantined;
                break;
        }

        if( target == nullptr || *target ) {
            return false;
        }
        *target = true;
        modifiers_quarantined = true;
        return true;
    }

    bool callback_quarantined( runtime_callback_kind kind ) const
    {
        switch( kind ) {
            case runtime_callback_kind::turn:
                return turn_quarantined;
            case runtime_callback_kind::locale:
                return locale_quarantined;
            case runtime_callback_kind::ui:
                return ui_quarantined;
        }
        return true;
    }
};
} // namespace ncmm
