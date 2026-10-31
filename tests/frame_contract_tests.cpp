#include "native_aa/frame_contract.hpp"

#include <cstdlib>
#include <iostream>
#include <limits>

using namespace native_aa;

namespace {
int failures = 0;
void expect(Admission actual, Admission wanted, const char* context) {
    if (actual == wanted) return;
    std::cerr << context << ": " << describe(actual) << '\n';
    ++failures;
}

Frame native_frame() {
    Frame f;
    f.session = 5; f.sequence = 1; f.device = 8;
    f.source = 21; f.destination = 22;
    f.input = f.output = {2560, 1440}; f.samples = 1;
    f.color = ColorDomain::DisplayReferredSdr; f.ui = UiBoundary::BeforeUi;
    f.native_scene_confirmed = true;
    f.source_complete = true; f.destination_available = true;
    return f;
}
} // namespace

int main() {
    BackendContract b;
    auto f = native_frame();
    expect(admit(false, f, b), Admission::Disabled, "Off needs no graphics resources");
    expect(admit(true, f, b), Admission::BackendUnavailable, "No shipped backend");
    b = {5, 8, true, 2560ULL * 1440 * 10};
    expect(admit(true, f, b), Admission::MayRecord, "Exact native scratch budget");
    --b.scratch_budget_bytes;
    expect(admit(true, f, b), Admission::ScratchBudgetExceeded, "One byte over budget");
    b.scratch_budget_bytes = std::numeric_limits<std::uint64_t>::max();

    f.input = {1920, 1080};
    expect(admit(true, f, b), Admission::NonNativeOrConflicting, "No hidden upscaling");
    f = native_frame(); f.native_scene_confirmed = false;
    expect(admit(true, f, b), Admission::NonNativeOrConflicting, "Equal dimensions are insufficient");
    f = native_frame(); f.other_reconstruction_active = true;
    expect(admit(true, f, b), Admission::NonNativeOrConflicting, "Avoid double reconstruction");
    for (const auto sample_count : {0U, 2U, 4U, 8U}) {
        f = native_frame(); f.samples = sample_count;
        expect(admit(true, f, b), Admission::UnresolvedMultisampling, "Resolved MSAA only");
    }
    for (const auto color : {ColorDomain::Unknown, ColorDomain::LinearHdr,
                             ColorDomain::Pq, ColorDomain::Hlg}) {
        f = native_frame(); f.color = color;
        expect(admit(true, f, b), Admission::UnsupportedColorDomain, "Do not guess HDR encoding");
    }
    for (const auto boundary : {UiBoundary::Unknown, UiBoundary::CompositedUi}) {
        f = native_frame(); f.ui = boundary;
        expect(admit(true, f, b), Admission::UnsafeUiBoundary, "Protect displays and UI");
    }
    f = native_frame(); f.session = 6;
    expect(admit(true, f, b), Admission::WrongSessionOrDevice, "Reject previous-session resources");
    f = native_frame(); f.device = 9;
    expect(admit(true, f, b), Admission::WrongSessionOrDevice, "Reject cross-device resources");
    f = native_frame(); f.source_complete = false;
    expect(admit(true, f, b), Admission::ResourcesBusy, "Source fence pending");
    f = native_frame(); f.destination_available = false;
    expect(admit(true, f, b), Admission::ResourcesBusy, "Output still presented");
    f = native_frame(); f.destination = f.source;
    expect(admit(true, f, b), Admission::InvalidFrame, "No in-place frame overwrite");
    f = native_frame(); f.sequence = 0;
    expect(admit(true, f, b), Admission::InvalidFrame, "No uninitialized frame");
    f = native_frame(); f.output.width = f.input.width = 0;
    expect(admit(true, f, b), Admission::InvalidFrame, "Minimized window");
    f = native_frame();
    f.input = f.output = {std::numeric_limits<std::uint32_t>::max(),
                          std::numeric_limits<std::uint32_t>::max()};
    expect(admit(true, f, b), Admission::ScratchBudgetExceeded, "Overflow cannot admit work");
    if (failures) return EXIT_FAILURE;
    std::cout << "Native-AA CPU frame-contract checks PASS; no graphics backend exercised\n";
    return EXIT_SUCCESS;
}
