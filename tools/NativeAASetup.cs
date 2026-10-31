// Copyright (c) 2026 Native AA contributors. MIT; see LICENSE.
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Security.Cryptography;
using System.Text;
using System.Web.Script.Serialization;
using System.Windows.Forms;
using Microsoft.Win32;

internal static class NativeAASetup {
    const string Version = "0.3.1-alpha";
    const string LayerKey = @"Software\Khronos\Vulkan\ImplicitLayers";
    static readonly JavaScriptSerializer Json = new JavaScriptSerializer { MaxJsonLength = 8000000 };
    static readonly UTF8Encoding Utf8 = new UTF8Encoding(false);
    static string Package { get { return AppDomain.CurrentDomain.BaseDirectory.TrimEnd(Path.DirectorySeparatorChar); } }
    static Dictionary<string, object> Object(object value) { return (Dictionary<string, object>)value; }
    static Dictionary<string, object> Read(string file) { return Object(Json.DeserializeObject(File.ReadAllText(file, Utf8))); }
    static string Text(Dictionary<string, object> value, string key) { return Convert.ToString(value[key]); }
    static object[] Items(Dictionary<string, object> value, string key) { return (object[])value[key]; }
    static bool Same(string a, string b) { return String.Equals(a, b, StringComparison.OrdinalIgnoreCase); }
    static string Hash(string file) {
        using (var sha = SHA256.Create()) using (var stream = File.OpenRead(file))
            return BitConverter.ToString(sha.ComputeHash(stream)).Replace("-", "");
    }
    static void Require(bool condition, string message) { if (!condition) throw new InvalidOperationException(message); }
    static void Write(string path, string text) { File.WriteAllText(path, text, Utf8); }
    static string Within(string root, string relative) {
        Require(!String.IsNullOrWhiteSpace(relative) && !Path.IsPathRooted(relative) && !relative.Contains(":") &&
            !relative.Replace('\\', '/').Split('/').Contains(".."), "Invalid installation path.");
        string result = Path.GetFullPath(Path.Combine(root, relative.Replace('/', Path.DirectorySeparatorChar)));
        Require(result.StartsWith(root + Path.DirectorySeparatorChar, StringComparison.OrdinalIgnoreCase), "Path escaped the selected directory.");
        string check = result;
        while (check != null && (Same(check, root) || check.StartsWith(root + Path.DirectorySeparatorChar, StringComparison.OrdinalIgnoreCase))) {
            if (File.Exists(check) || Directory.Exists(check))
                Require((File.GetAttributes(check) & FileAttributes.ReparsePoint) == 0, "Redirected files/directories are not supported: " + check);
            check = Path.GetDirectoryName(check);
        }
        return result;
    }
    static string Simulator(string path) {
        string root = Path.GetFullPath(path).TrimEnd(Path.DirectorySeparatorChar, Path.AltDirectorySeparatorChar);
        Require(File.Exists(Path.Combine(root, "X-Plane.exe")) && Directory.Exists(Path.Combine(root, "Resources", "plugins")),
            "Select the X-Plane 12 root folder containing X-Plane.exe and Resources/plugins.");
        Require(Process.GetProcessesByName("X-Plane").Length == 0, "Close X-Plane normally before installing, updating or uninstalling Native AA.");
        return root;
    }
    static string Destination(string root) { return Path.Combine(root, "Resources", "plugins", "NativeAA"); }
    static void VerifyPackage() {
        var manifest = Read(Path.Combine(Package, "RELEASE-MANIFEST.json"));
        Require(Text(manifest, "version") == Version && Text(manifest, "platform") == "Windows-x64", "Incorrect package version/platform.");
        foreach (object entry in Items(manifest, "files")) {
            var file = Object(entry); string path = Within(Package, Text(file, "path"));
            Require(File.Exists(path) && Same(Hash(path), Text(file, "sha256")), "Package file missing or changed: " + Text(file, "path"));
        }
        var build = Read(Path.Combine(Package, "build/live-layer/build-receipt.json"));
        var test = Read(Path.Combine(Package, "build/live-layer/test-receipt.json"));
        var activation = Read(Path.Combine(Package, "build/live-layer/activation-receipt.json"));
        var ui = Read(Path.Combine(Package, "build/plugin/build-receipt.json"));
        string dll = Hash(Path.Combine(Package, "build/live-layer/native-aa-live.dll"));
        foreach (var receipt in new[] { build, test, activation, ui }) Require(Text(receipt, "status") == "PASS", "Successful build/test receipts required.");
        foreach (var receipt in new[] { build, test, activation }) Require(Same(dll, Text(receipt, "dllSHA256")), "Renderer does not match its tests.");
        Require(Same(Hash(Path.Combine(Package, "build/plugin/NativeAA/64/win.xpl")), Text(ui, "pluginSHA256")), "Plugin does not match checked source.");
        Require(Same(Hash(Path.Combine(Package, "tools/LayerManifest.ps1")), Text(activation, "manifestSourceSHA256")), "Manifest validation is stale.");
        foreach (var receipt in new[] { build, ui }) foreach (object entry in Items(receipt, "inputs")) {
            var file = Object(entry);
            Require(Same(Hash(Within(Package, Text(file, "path"))), Text(file, "sha256")), "Build input changed: " + Text(file, "path"));
        }
    }
    static Dictionary<string, object> Inventory(string destination, string root) {
        string path = Within(destination, "installation.json");
        if (!File.Exists(path)) return null;
        var inventory = Read(path);
        Require(Text(inventory, "product") == "NativeAA" && Same(Text(inventory, "simulator"), root), "Installation inventory does not match this simulator.");
        foreach (object entry in Items(inventory, "files")) {
            var file = Object(entry); Within(destination, Text(file, "path"));
            string hash = Text(file, "sha256");
            Require(hash.Length == 64 && hash.All(Uri.IsHexDigit), "Invalid installation hash.");
        }
        return inventory;
    }
    static Dictionary<string, object> Record(string path, string hash) {
        return new Dictionary<string, object> { { "path", path }, { "sha256", hash } };
    }
    static Dictionary<string, string> Payload() {
        var map = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase) {
            { "64/win.xpl", "build/plugin/NativeAA/64/win.xpl" },
            { "README.txt", "docs/CONTROLS.md" },
            { "notices/NativeAA-MIT.txt", "LICENSE" },
            { "notices/SMAA-MIT.txt", "third_party/smaa/LICENSE.txt" },
            { "notices/FXAA-source-and-BSD-notice.txt", "third_party/fxaa/Fxaa3_11.h" },
            { "notices/FXAA-Intel-Apache.txt", "third_party/fxaa/CMAA2-license.txt" },
            { "notices/XPLM-SDK-license.txt", "third_party/XPLM-SDK-license.txt" }
        };
        string dll = Hash(Path.Combine(Package, "build/live-layer/native-aa-live.dll"));
        map.Add("renderer/native-aa-" + dll.Substring(0, 12).ToLowerInvariant() + ".dll", "build/live-layer/native-aa-live.dll");
        string shaderRoot = Path.Combine(Package, "build/vulkan/shaders");
        var shaders = Directory.GetFiles(shaderRoot).Where(p => System.Text.RegularExpressions.Regex.IsMatch(Path.GetFileName(p),
            @"^(live-smaa-[0-3]-[0-2]|fxaa-[02]|native-taa|native-depth-(1|ms))\.comp\.spv$" )).ToArray();
        Require(shaders.Length == 17, "Expected 17 production shaders.");
        foreach (string file in shaders) map.Add("renderer/shaders/" + Path.GetFileName(file), "build/vulkan/shaders/" + Path.GetFileName(file));
        return map;
    }
    internal static void Install(string selected, bool registration) {
        string root = Simulator(selected); VerifyPackage();
        string destination = Destination(root); var previous = Inventory(destination, root); var payload = Payload();
        var owned = new List<string>(payload.Keys); owned.Add("renderer/native-aa.json");
        if (registration || File.Exists(Path.Combine(destination, "standalone.enabled"))) owned.Add("standalone.enabled");
        foreach (string path in owned) Within(destination, path);
        foreach (var pair in payload) {
            string target = Within(destination, pair.Key); Directory.CreateDirectory(Path.GetDirectoryName(target));
            if (pair.Key.EndsWith(".dll") && File.Exists(target)) Require(Same(Hash(target), Hash(Within(Package, pair.Value))), "Existing hash-named renderer changed.");
            else File.Copy(Within(Package, pair.Value), target, true);
        }
        string dllName = Path.GetFileName(payload.Keys.Single(k => k.EndsWith(".dll")));
        string manifestPath = Within(destination, "renderer/native-aa.json");
        var layer = new Dictionary<string, object> {
            { "name", "VK_LAYER_NATIVE_AA_live_test" }, { "type", "GLOBAL" }, { "library_path", ".\\" + dllName },
            { "api_version", "1.3.0" }, { "implementation_version", "2" },
            { "description", "Independent native scene AA; exact selected simulator only" },
            { "functions", new Dictionary<string, object> { { "vkNegotiateLoaderLayerInterfaceVersion", "aaNegotiate" } } },
            { "disable_environment", new Dictionary<string, object> { { "NATIVE_AA_DISABLE", "1" } } }
        };
        Write(manifestPath, Json.Serialize(new Dictionary<string, object> { { "file_format_version", "1.2.0" }, { "layer", layer } }));
        if (registration) {
            Write(Within(destination, "standalone.enabled"), "NativeAAStandalone=1\n");
            using (var key = Registry.CurrentUser.CreateSubKey(LayerKey)) key.SetValue(manifestPath, 0, RegistryValueKind.DWord);
        }
        if (previous != null) foreach (object entry in Items(previous, "files")) {
            var file = Object(entry); string relative = Text(file, "path").Replace('\\', '/');
            if (owned.Any(p => Same(p, relative)) || Same(relative, "installation.json")) continue;
            string path = Within(destination, relative);
            if (File.Exists(path) && Same(Hash(path), Text(file, "sha256"))) File.Delete(path);
        }
        var records = owned.Select(p => Record(p, Hash(Within(destination, p)))).ToArray();
        Write(Within(destination, "installation.json"), Json.Serialize(new Dictionary<string, object> {
            { "product", "NativeAA" }, { "version", Version }, { "simulator", root }, { "files", records }
        }));
    }
    internal static void Uninstall(string selected, bool registration) {
        string root = Simulator(selected); string destination = Destination(root); var inventory = Inventory(destination, root);
        Require(inventory != null, "No Native AA installation inventory found. No files removed.");
        if (registration) using (var key = Registry.CurrentUser.OpenSubKey(LayerKey, true)) {
            if (key != null) key.DeleteValue(Within(destination, "renderer/native-aa.json"), false);
        }
        foreach (object entry in Items(inventory, "files")) {
            var file = Object(entry); string path = Within(destination, Text(file, "path"));
            if (File.Exists(path) && Same(Hash(path), Text(file, "sha256"))) File.Delete(path);
        }
        File.Delete(Within(destination, "installation.json"));
        foreach (string relative in new[] { "renderer/shaders", "renderer", "notices", "64", "" }) {
            string folder = relative.Length == 0 ? destination : Within(destination, relative);
            if (Directory.Exists(folder) && !Directory.EnumerateFileSystemEntries(folder).Any()) Directory.Delete(folder);
        }
    }
    [STAThread] static int Main(string[] args) {
        try {
            if (args.Length > 0) {
                Require((args.Length == 2 || (args.Length == 3 && args[2] == "--no-registration")) &&
                    (args[0] == "--install" || args[0] == "--uninstall"), "Usage: NativeAA-Setup.exe --install|--uninstall <simulator> [--no-registration]");
                bool registration = args.Length == 2;
                if (args[0] == "--install") Install(args[1], registration); else Uninstall(args[1], registration);
                return 0;
            }
            Application.EnableVisualStyles(); Application.SetCompatibleTextRenderingDefault(false);
            var form = new Form { Text = "Make XP Anti-Aliasing Great Again - " + Version, Width = 650, Height = 320,
                StartPosition = FormStartPosition.CenterScreen, FormBorderStyle = FormBorderStyle.FixedDialog, MaximizeBox = false };
            string icon = Path.Combine(Package, "assets/branding/aa-icon.ico");
            if (File.Exists(icon)) form.Icon = new System.Drawing.Icon(icon);
            var introduction = new Label { Left = 22, Top = 22, Width = 590, Height = 60,
                Text = "Native-resolution SMAA, FXAA and experimental TAA for X-Plane 12.\nClose X-Plane, select its root folder, then install. No administrator needed." };
            var pathBox = new TextBox { Left = 22, Top = 94, Width = 475 };
            var browse = new Button { Left = 510, Top = 91, Width = 100, Text = "Browse..." };
            var install = new Button { Left = 22, Top = 144, Width = 200, Height = 36, Text = "Install / update" };
            var uninstall = new Button { Left = 238, Top = 144, Width = 180, Height = 36, Text = "Uninstall" };
            var status = new Label { Left = 22, Top = 198, Width = 590, Height = 65, Text = "Read README-FIRST.txt for settings, alpha limits and license notices." };
            browse.Click += delegate {
                using (var picker = new FolderBrowserDialog { Description = "Select the folder containing X-Plane.exe", ShowNewFolderButton = false })
                    if (picker.ShowDialog(form) == DialogResult.OK) pathBox.Text = picker.SelectedPath;
            };
            Action<bool> run = delegate(bool remove) {
                install.Enabled = uninstall.Enabled = browse.Enabled = false;
                try {
                    status.Text = remove ? "Removing recorded Native AA files..." : "Checking package and installing..."; status.Refresh();
                    if (remove) Uninstall(pathBox.Text, true); else Install(pathBox.Text, true);
                    status.Text = remove ? "Uninstalled. Preferences and user-modified files are retained." :
                        "Installed! Launch normally, then Plugins > Native AA > Open AA controls. Set render input to Full.";
                } catch (Exception error) { status.Text = "Unable to complete. See the error message."; MessageBox.Show(form, error.Message, "Native AA setup", MessageBoxButtons.OK, MessageBoxIcon.Error); }
                finally { install.Enabled = uninstall.Enabled = browse.Enabled = true; }
            };
            install.Click += delegate { run(false); }; uninstall.Click += delegate { run(true); };
            form.Controls.AddRange(new Control[] { introduction, pathBox, browse, install, uninstall, status });
            Application.Run(form); return 0;
        } catch (Exception error) { Console.Error.WriteLine(error.Message); return 1; }
    }
}
