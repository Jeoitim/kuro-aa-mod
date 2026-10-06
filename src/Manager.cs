// SPDX-License-Identifier: MIT
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Drawing;
using System.IO;
using System.Linq;
using System.Text;
using System.Security.Cryptography;
using System.Windows.Forms;

internal static class Config
{
    internal static readonly string Root = AppDomain.CurrentDomain.BaseDirectory;
    internal static string PathOf(string relative) { return Path.Combine(Root, relative); }
    internal static void RequireStopped()
    {
        if (!File.Exists(PathOf("ed9.exe"))) return;
        foreach (var process in Process.GetProcessesByName("ed9")) using (process) {
            string runningPath;
            try { runningPath = process.MainModule.FileName; }
            catch (System.ComponentModel.Win32Exception) { throw new InvalidOperationException("Unable to verify game state. Close the game first."); }
            if (string.Equals(runningPath, PathOf("ed9.exe"), StringComparison.OrdinalIgnoreCase))
                throw new InvalidOperationException("Close the game before changing the backend or injection switch.");
        }
    }
    internal static Dictionary<string, string> Read(string file)
    {
        var values = new Dictionary<string, string>();
        if (!File.Exists(file)) return values;
        foreach (string line in File.ReadAllLines(file)) {
            int equals = line.IndexOf('=');
            if (equals > 0) values[line.Substring(0, equals).Trim()] = line.Substring(equals + 1).Trim();
        }
        return values;
    }
    internal static void Set(string file, string section, Dictionary<string, string> changes)
    {
        var lines = File.Exists(file) ? File.ReadAllLines(file).ToList() : new List<string>();
        int start = lines.FindIndex(s => s.Trim() == "[" + section + "]");
        if (start < 0) { lines.Add("[" + section + "]"); start = lines.Count - 1; }
        int end = lines.FindIndex(start + 1, s => s.TrimStart().StartsWith("["));
        if (end < 0) end = lines.Count;
        foreach (var change in changes) {
            int index = -1;
            for (int i = start + 1; i < end; i++) {
                int equals = lines[i].IndexOf('=');
                if (equals > 0 && lines[i].Substring(0, equals).Trim() == change.Key) { index = i; break; }
            }
            string value = change.Key + "=" + change.Value;
            if (index >= 0) lines[index] = value;
            else { lines.Insert(end, value); end++; }
        }
        string temp = file + ".kuro.tmp";
        File.WriteAllLines(temp, lines, new UTF8Encoding(false));
        if (File.Exists(file)) File.Replace(temp, file, null); else File.Move(temp, file);
    }
    internal static void SelectBackend(int backend, int quality, int preset, bool jitter, bool ui, float sharpness)
    {
        RequireStopped();
        if (backend < 0 || backend > 4 || quality < 0 || quality > 5 || preset < 0 || preset > 2)
            throw new ArgumentOutOfRangeException("backend");
        bool tfaa = backend == 3;
        bool enabled = backend != 4;
        Set(PathOf("AeonSR.ini"), "AeonSR", new Dictionary<string, string> {
            { "Enabled", enabled && !tfaa ? "1" : "0" },
            { "Upscaler", tfaa || !enabled ? "3" : backend.ToString() },
            { "UpscaleMode", quality.ToString() }, { "SpatialJitter", jitter && enabled && !tfaa ? "1" : "0" },
            { "KeepInterface", ui ? "1" : "0" }, { "NeuralRender", "0" },
            { "Sharpness", sharpness.ToString(System.Globalization.CultureInfo.InvariantCulture) }
        });
        string name = tfaa ? new[] { "Stable", "Balanced", "Sharp" }[preset] : "Native";
        Set(PathOf("ReShade.ini"), "GENERAL", new Dictionary<string, string> {
            { "PresetPath", ".\\KuroTFAA\\Presets\\" + name + ".ini" },
            { "StartupPresetPath", ".\\KuroTFAA\\Presets\\" + name + ".ini" }
        });
    }
    internal static void ToggleInjection(bool enabled)
    {
        RequireStopped();
        string on = PathOf("dxgi.dll"), off = PathOf("dxgi.dll.kuro-disabled");
        string candidate = File.Exists(on) ? on : off;
        if (!File.Exists(candidate)) throw new FileNotFoundException("No Mod injection file found.");
        using (var hash = SHA256.Create()) using (var stream = File.OpenRead(candidate)) {
            string actual = BitConverter.ToString(hash.ComputeHash(stream)).Replace("-", "");
            if (actual != "0CEE63F9C9F13F3AC909C5B4903F4DBB4B719A7AB3B4F13B0DEAF83C814B94F7")
                throw new IOException("This injection DLL is not owned by this Mod. No files changed.");
        }
        if (enabled && File.Exists(off) && !File.Exists(on)) File.Move(off, on);
        else if (!enabled && File.Exists(on) && !File.Exists(off)) File.Move(on, off);
        else if ((enabled && File.Exists(on)) || (!enabled && File.Exists(off))) return;
        else throw new IOException("Injection file state is inconsistent. Check the installation.");
    }
}

internal sealed class Manager : Form
{
    private readonly ComboBox backend = new ComboBox(), quality = new ComboBox(), preset = new ComboBox();
    private readonly CheckBox jitter = new CheckBox(), ui = new CheckBox();
    private readonly NumericUpDown sharp = new NumericUpDown();
    private readonly Label status = new Label();
    internal Manager()
    {
        Text = "Kuro AA Mod"; ClientSize = new Size(520, 360); MinimumSize = new Size(536, 399);
        StartPosition = FormStartPosition.CenterScreen; Font = new Font("Segoe UI", 10);
        BackColor = Color.FromArgb(246, 248, 250);
        var layout = new TableLayoutPanel { Dock = DockStyle.Fill, Padding = new Padding(22), ColumnCount = 2, RowCount = 8 };
        layout.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute, 155)); layout.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100));
        Controls.Add(layout);
        backend.Items.AddRange(new object[] { "NVIDIA DLAA / DLSS", "AMD FSR Native AA / SR", "Intel XeSS AA / SR", "TFAA", "Effects off" });
        quality.Items.AddRange(new object[] { "Native AA", "Ultra Quality", "Quality", "Balanced", "Performance", "Ultra Performance" });
        preset.Items.AddRange(new object[] { "Stable", "Balanced", "Sharp" });
        foreach (var combo in new[] { backend, quality, preset }) { combo.DropDownStyle = ComboBoxStyle.DropDownList; combo.Dock = DockStyle.Fill; }
        var aeon = Config.Read(Config.PathOf("AeonSR.ini")); var reshade = Config.Read(Config.PathOf("ReShade.ini"));
        string activePreset = reshade.ContainsKey("PresetPath") ? reshade["PresetPath"] : "";
        bool tfaa = activePreset.Length > 0 && activePreset.IndexOf("Native.ini", StringComparison.OrdinalIgnoreCase) < 0;
        int b = 0, q = 0; if (aeon.ContainsKey("Upscaler")) int.TryParse(aeon["Upscaler"], out b);
        if (aeon.ContainsKey("UpscaleMode")) int.TryParse(aeon["UpscaleMode"], out q);
        backend.SelectedIndex = tfaa ? 3 : aeon.ContainsKey("Enabled") && aeon["Enabled"] == "0" ? 4 : Math.Min(b, 2);
        quality.SelectedIndex = Math.Max(0, Math.Min(q, 5));
        preset.SelectedIndex = activePreset.IndexOf("Sharp.ini", StringComparison.OrdinalIgnoreCase) >= 0 ? 2 : activePreset.IndexOf("Balanced.ini", StringComparison.OrdinalIgnoreCase) >= 0 ? 1 : 0;
        jitter.Text = "Scene jitter (experimental)"; jitter.Checked = aeon.ContainsKey("SpatialJitter") && aeon["SpatialJitter"] == "1";
        ui.Text = "Legacy UI detection"; ui.Checked = !aeon.ContainsKey("KeepInterface") || aeon["KeepInterface"] == "1";
        jitter.AutoSize = true; ui.AutoSize = true;
        jitter.Anchor = AnchorStyles.Left; ui.Anchor = AnchorStyles.Left;
        sharp.DecimalPlaces = 2; sharp.Increment = 0.01m; sharp.Minimum = 0; sharp.Maximum = 0.30m; sharp.Dock = DockStyle.Left;
        decimal amount; if (aeon.ContainsKey("Sharpness") && decimal.TryParse(aeon["Sharpness"], System.Globalization.NumberStyles.Float, System.Globalization.CultureInfo.InvariantCulture, out amount)) sharp.Value = Math.Max(0, Math.Min(0.30m, amount));
        AddRow(layout, 0, "Backend", backend); AddRow(layout, 1, "Resolution mode", quality); AddRow(layout, 2, "TFAA preset", preset);
        AddRow(layout, 3, "Sharpening", sharp); AddRow(layout, 4, "Rendering", jitter); AddRow(layout, 5, "Interface", ui);
        var actions = new FlowLayoutPanel { Dock = DockStyle.Fill, AutoSize = true, WrapContents = true };
        Button apply = ButtonOf("Apply", () => { Config.SelectBackend(backend.SelectedIndex, quality.SelectedIndex, preset.SelectedIndex, jitter.Checked, ui.Checked, (float)sharp.Value); status.Text = "Saved. Ready for next launch."; });
        Button toggle = ButtonOf("Disable / enable", () => { Config.ToggleInjection(!File.Exists(Config.PathOf("dxgi.dll"))); status.Text = File.Exists(Config.PathOf("dxgi.dll")) ? "Injection enabled" : "Injection disabled"; });
        Button launch = ButtonOf("Launch game", () => { if (!File.Exists(Config.PathOf("ed9.exe"))) throw new FileNotFoundException("Place the Mod beside ed9.exe first."); Process.Start(new ProcessStartInfo(Config.PathOf("ed9.exe")) { WorkingDirectory = Config.Root, UseShellExecute = true }); });
        Button logs = ButtonOf("Diagnostics", () => { string path = Config.PathOf("AeonSR.log"); if (!File.Exists(path)) path = Config.PathOf("ReShade.log"); if (!File.Exists(path)) throw new FileNotFoundException("No runtime log yet."); Process.Start(path); });
        Button sceneMode = ButtonOf("Scene mode", () => { using (var dialog = new SceneSettings()) dialog.ShowDialog(this); });
        actions.Controls.AddRange(new Control[] { apply, toggle, launch, logs, sceneMode }); layout.Controls.Add(actions, 0, 6); layout.SetColumnSpan(actions, 2);
        status.Text = File.Exists(Config.PathOf("dxgi.dll")) ? "Injection enabled" : "Injection disabled"; status.AutoSize = true; status.Dock = DockStyle.Fill; layout.Controls.Add(status, 0, 7); layout.SetColumnSpan(status, 2);
        backend.SelectedIndexChanged += delegate { RefreshControls(); }; RefreshControls();
    }
    private void RefreshControls() { quality.Enabled = backend.SelectedIndex < 3; preset.Enabled = backend.SelectedIndex == 3; jitter.Enabled = backend.SelectedIndex < 3; ui.Enabled = backend.SelectedIndex < 3; sharp.Enabled = backend.SelectedIndex < 3; }
    private static void AddRow(TableLayoutPanel layout, int row, string label, Control control) { layout.RowStyles.Add(new RowStyle(SizeType.Absolute, 36)); layout.Controls.Add(new Label { Text = label, AutoSize = true, Anchor = AnchorStyles.Left }, 0, row); layout.Controls.Add(control, 1, row); }
    private static Button ButtonOf(string label, Action action) { var button = new Button { Text = label, AutoSize = true, Height = 32, FlatStyle = FlatStyle.System }; button.Click += delegate { try { action(); } catch (Exception e) { MessageBox.Show(e.Message, "Kuro AA Mod", MessageBoxButtons.OK, MessageBoxIcon.Information); } }; return button; }
}

internal sealed class SceneSettings : Form
{
    private readonly CheckBox enabled = new CheckBox(), capture = new CheckBox();
    internal SceneSettings()
    {
        Text = "Scene-only AA"; ClientSize = new Size(400, 160); FormBorderStyle = FormBorderStyle.FixedDialog;
        MaximizeBox = false; MinimizeBox = false; StartPosition = FormStartPosition.CenterParent; Font = new Font("Segoe UI", 10);
        var values = Config.Read(Config.PathOf("KuroUI.ini"));
        enabled.Text = "Process scene before UI (experimental)"; enabled.AutoSize = true; enabled.Location = new Point(20,20);
        enabled.Checked = values.ContainsKey("EnableEarlyAA") && values["EnableEarlyAA"] == "1";
        capture.Text = "Capture draw targets for diagnostics"; capture.AutoSize = true; capture.Location = new Point(20,55);
        capture.Checked = values.ContainsKey("CaptureCandidates") && values["CaptureCandidates"] == "1";
        var apply = new Button { Text = "Apply", Location = new Point(290,105), Size = new Size(90,30) };
        apply.Click += delegate {
            try {
                Config.RequireStopped();
                if (!File.Exists(Config.PathOf("KuroUI.addon64"))) throw new FileNotFoundException("Native scene integration component is missing.");
                Config.Set(Config.PathOf("KuroUI.ini"), "KuroUI", new Dictionary<string,string> {
                    { "EnableEarlyAA", enabled.Checked ? "1" : "0" }, { "EarlyUIShaderHash", "23f7ff8def7a9871" },
                    { "EarlyUIVertexShaderHash", "323e5c4e7ef5ce9" }, { "AllowOffscreenTarget", "1" },
                    { "SkipUnmatchedFrames", "1" }, { "CaptureCandidates", capture.Checked ? "1" : "0" }
                });
                if (enabled.Checked) Config.Set(Config.PathOf("AeonSR.ini"), "AeonSR", new Dictionary<string,string> { { "KeepInterface","0" }, { "SpatialJitter","0" }, { "UpscaleEffects","0" } });
                Close();
            } catch(Exception e) { MessageBox.Show(e.Message,"Kuro AA Mod",MessageBoxButtons.OK,MessageBoxIcon.Information); }
        };
        Controls.AddRange(new Control[] { enabled,capture,apply });
    }
}

internal static class Program
{
    [STAThread] private static int Main(string[] args)
    {
        try {
            if (args.Length == 2 && args[0] == "--render-test") {
                Application.EnableVisualStyles();
                using (var form = new Manager()) using (var bitmap = new Bitmap(form.Width, form.Height)) {
                    form.StartPosition = FormStartPosition.Manual; form.Location = new Point(-10000, -10000); form.ShowInTaskbar = false;
                    form.Show(); Application.DoEvents(); form.PerformLayout();
                    form.DrawToBitmap(bitmap, new Rectangle(0, 0, bitmap.Width, bitmap.Height));
                    bitmap.Save(args[1], System.Drawing.Imaging.ImageFormat.Png);
                    form.Close();
                }
                return 0;
            }
            if (args.Length == 1 && args[0] == "--self-test") {
                string path = Config.PathOf("manager-self-test.ini");
                if (File.Exists(path)) throw new IOException("Self-test temporary file exists");
                try {
                    File.WriteAllText(path,"[GENERAL]\nKeep=42\n[AeonSR]\nEnabled=0\n");
                    Config.Set(path,"AeonSR",new Dictionary<string,string> { { "Enabled","1" }, { "Upscaler","2" } });
                    var values = Config.Read(path);
                    if (values["Keep"] != "42" || values["Enabled"] != "1" || values["Upscaler"] != "2") throw new Exception("Configuration test failed");
                } finally { if (File.Exists(path)) File.Delete(path); }
                Console.WriteLine("Manager config preservation test passed"); return 0;
            }
            if (args.Length == 2 && args[0] == "--backend") {
                int backend = Array.IndexOf(new[] { "dlss", "fsr", "xess", "tfaa", "off" },args[1].ToLowerInvariant());
                Config.SelectBackend(backend,0,0,false,true,0); return 0;
            }
            if (args.Length == 1 && (args[0] == "--disable" || args[0] == "--enable")) { Config.ToggleInjection(args[0] == "--enable"); return 0; }
            if (args.Length != 0) throw new ArgumentException("Unknown argument");
            Application.EnableVisualStyles(); Application.SetCompatibleTextRenderingDefault(false); Application.Run(new Manager()); return 0;
        } catch (Exception e) { Console.Error.WriteLine(e.Message); return 1; }
    }
}
