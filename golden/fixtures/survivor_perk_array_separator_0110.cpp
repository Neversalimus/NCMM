#include <array>
#include <cstddef>

struct perk_def {
    const char *id;
    int branch;
};

// HOTFIX9 regression fixture for the real append chain:
// 0.9.15 Prime tail -> 0.10.0 Mechanical -> 0.11.0 Reactive.
// Each former tail may start comma-less, but once another block is appended
// the generator must supply exactly one separator.
static const perk_def perks[] = {
    { "secx_prime_vessel", 5 },
    { "cm_critical_eye", 0 },
    { "am_apex_adaptation", 5 },
    { "cr_riposte", 0 }
};

int main()
{
    return std::size( perks ) == 4 && perks[1].id[0] == 'c' && perks[3].id[0] == 'c' ? 0 : 1;
}
