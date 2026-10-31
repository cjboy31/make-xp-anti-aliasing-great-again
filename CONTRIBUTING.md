# Contributing

This is an early research project. Start with an issue describing the scene
boundary, graphics API, simulator version and evidence your change will use.
Small reviewable pull requests are welcome. Run the portable checks in README.

Keep FSR, vendor neural models and frame-generation dependencies out of this
AA-only tool. Do not copy restricted add-on code, DLLs or simulator assets.
Review licenses before adding shaders or lookup tables and preserve notices.
Never upload personal settings, flight logs, hardware identifiers or credentials
as source. Share only redacted diagnostics you are authorized to publish.

Graphics changes need actual API validation, resource-lifetime tests and matched
simulator comparisons. CI CPU tests are not evidence of correct rendered output.
Document failures and unsupported paths. Do not report AA active from a saved
checkbox, loaded plugin, successful allocation or admission check.

Make in-sim controls accessible with tooltips, clear selected states, live/next
session labels and explicit bypass reasons. Do not read controllers, take camera
control, silently change unrelated settings or require a dedicated sim launcher.
Before a release, verify install/update/removal and preservation of user choices
on every platform claimed. Do not publish an installer for a nonexistent backend.
