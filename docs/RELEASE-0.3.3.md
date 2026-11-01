# Native AA 0.3.3 — compatibility update

This update fixes a case where normal graphics work from the Zink plugin bridge
could make Native AA stop processing. It also gives a more specific reason when
a drawing path cannot be handled safely. The picture stays unchanged while AA
is bypassed.

Keep using **Plugins > Native AA > Open AA controls**. Choose Native AA on
**Renderer**, close X-Plane normally and reopen it if you changed that selection.
Use **Full** input scale. Full is needed for Native AA, but cannot clear a
compatibility warning on its own. Both FSR and Native AA can remain installed;
choose one image processor per session.

If Zink still prevents AA from becoming Active, share the exact warning and the
newest log in **X-Plane > Output > NativeAA**, plus X-Plane's **Log.txt** from the
same launch. The [report page](https://xplane-mod-log-reports.cjboy31.chatgpt.site/)
lets you preview filtered copies before sending them to Discord. Enter the
version shown in the Changelog tab. Native AA does not automatically upload logs.

As a comparison, you can turn **Use Zink plugin bridge** Off in X-Plane Graphics,
then close and reopen the sim. Some add-ons need Zink, so restore it if their
screens or performance get worse.

The correction passes Windows GPU checks and Linux automated checks. It does not
prove every Zink/add-on combination is fixed. A full Windows Zink flight and
Linux flight checks still need validation. Linux remains a separately labeled
beta. Mac rendering is unavailable. No new AA algorithm or FPS gain is claimed.

Close X-Plane before updating. Extract the complete Windows ZIP and run
**NativeAA-Setup.exe**, or extract the full Linux beta and run **./install.sh**.
Select your own simulator folder. Saved choices are kept.
