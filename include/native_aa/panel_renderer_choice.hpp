// SPDX-License-Identifier: MIT
#pragma once
#include "renderer_selection.hpp"

namespace native_aa {
// Display the existing automatic policy without writing preferences or claiming
// a session. Only a deliberate click should change the next launch.
inline xp_renderer::Choice displayed_renderer_choice(const std::filesystem::path& root){
    const auto saved=xp_renderer::read(root);
    if(saved!=xp_renderer::Choice::automatic)return saved;
    return xp_renderer::aa_installed(root)?xp_renderer::Choice::native_aa:xp_renderer::Choice::fsr;
}
}
