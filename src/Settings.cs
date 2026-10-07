// SPDX-License-Identifier: MIT
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Drawing;
using System.IO;
using System.Linq;
using System.Runtime.InteropServices;
using System.Security.Cryptography;
using System.Text;
using System.Windows.Forms;
using System.Reflection;

[assembly: AssemblyTitle("黎之轨迹抗锯齿设置")]
[assembly: AssemblyProduct("Kuro AA Mod")]
[assembly: AssemblyVersion("0.3.1.0")]
[assembly: AssemblyFileVersion("0.3.1.1")]

internal static class Config
{
    internal static readonly string ModRoot = AppDomain.CurrentDomain.BaseDirectory;
    internal static readonly string Root = Directory.GetParent(ModRoot.TrimEnd(Path.DirectorySeparatorChar)).FullName;
    internal static string PathOf(string relative) { return Path.Combine(relative=="ed9.exe"||relative=="ReShade.ini"||relative.StartsWith("dxgi.dll")?Root:ModRoot, relative); }
    internal static void RequireStopped()
    {
        if (!File.Exists(PathOf("ed9.exe"))) return;
        foreach (var process in Process.GetProcessesByName("ed9")) using (process) {
            string path;
            try { path = process.MainModule.FileName; }
            catch (System.ComponentModel.Win32Exception) { throw new InvalidOperationException("无法确认游戏状态，请先退出游戏。"); }
            if (string.Equals(path, PathOf("ed9.exe"), StringComparison.OrdinalIgnoreCase))
                throw new InvalidOperationException("请先退出游戏，再保存设置或切换注入状态。");
        }
    }
    internal static Dictionary<string,string> Read(string file)
    {
        var values = new Dictionary<string,string>();
        if (!File.Exists(file)) return values;
        foreach (string line in File.ReadAllLines(file)) {
            int equals = line.IndexOf('=');
            if (equals > 0 && !line.TrimStart().StartsWith(";")) values[line.Substring(0,equals).Trim()] = line.Substring(equals+1).Trim();
        }
        return values;
    }
    internal static bool Flag(Dictionary<string,string> values,string key,bool fallback)
    { return values.ContainsKey(key) ? values[key] == "1" : fallback; }
    internal static void Set(string file,string section,Dictionary<string,string> changes)
    {
        var lines = File.Exists(file) ? File.ReadAllLines(file).ToList() : new List<string>();
        int start = lines.FindIndex(s => s.Trim() == "["+section+"]");
        if (start < 0) { lines.Add("["+section+"]"); start = lines.Count-1; }
        int end = lines.FindIndex(start+1,s => s.TrimStart().StartsWith("["));
        if (end < 0) end = lines.Count;
        foreach (var change in changes) {
            int index = -1;
            for (int i=start+1;i<end;i++) {
                int equals = lines[i].IndexOf('=');
                if (equals > 0 && lines[i].Substring(0,equals).Trim() == change.Key) { index=i; break; }
            }
            string value = change.Key+"="+change.Value;
            if (index >= 0) lines[index]=value; else { lines.Insert(end,value); end++; }
        }
        string temp=file+".kuro.tmp";
        File.WriteAllLines(temp,lines,new UTF8Encoding(false));
        if (File.Exists(file)) File.Replace(temp,file,null); else File.Move(temp,file);
    }
    internal static void SelectBackend(int backend,int quality,float sharpness,int motionQuality=1)
    {
        RequireStopped();
        if (backend<0 || backend>4 || quality<0 || quality>5 || motionQuality<0 || motionQuality>1) throw new ArgumentException("抗锯齿选项无效。");
        if(float.IsNaN(sharpness) || float.IsInfinity(sharpness) || sharpness<0 || sharpness>1)throw new ArgumentException("锐化强度可填写 0.00 到 1.00。");
        Set(PathOf("AeonSR.ini"),"AeonSR",new Dictionary<string,string> {
            {"Enabled",backend==3 ? "0":"1"}, {"Upscaler",backend==4 ? "4294967295":backend.ToString()},
            {"UpscaleMode",quality.ToString()}, {"SpatialJitter","0"}, {"InternalFlowQuality",motionQuality.ToString()},
            {"KeepInterface","0"}, {"NeuralRender","0"}, {"UpscaleEffects","0"},
            {"Sharpness",sharpness.ToString(System.Globalization.CultureInfo.InvariantCulture)}
        });
        Set(PathOf("ReShade.ini"),"GENERAL",new Dictionary<string,string> {
            {"PresetPath",".\\KuroAA\\Native.ini"}, {"StartupPresetPath",".\\KuroAA\\Native.ini"}
        });
    }
    internal static int ReadRule(Dictionary<string,string> values)
    {
        string value;if(values.TryGetValue("AARule",out value))return value=="shader"?1:value=="full"?2:0;
        if(values.TryGetValue("EngineUIFunctionRVA",out value)&&value!="0"&&value!="0x0"&&value!="")return 0;
        if(Flag(values,"EnableEarlyAA",false))return 1;
        return values.ContainsKey("SkipUnmatchedFrames")&&!Flag(values,"SkipUnmatchedFrames",true)?2:0;
    }
    internal static void SetRule(int rule,bool capture)
    {
        RequireStopped();
        if(rule<0||rule>2)throw new ArgumentException("AA 规则无效。");
        if (!File.Exists(PathOf("KuroUI.addon64"))) throw new FileNotFoundException("未找到本抗锯齿 Mod 的场景组件，请检查安装文件。");
        Set(PathOf("KuroUI.ini"),"KuroUI",new Dictionary<string,string> {
            {"AARule",new[]{"engine","shader","full"}[rule]},
            {"EnableEarlyAA",rule==2 ? "0":"1"}, {"EarlyUIShaderHash",rule==1?"23f7ff8def7a9871":"0"},
            {"EarlyUIVertexShaderHash",rule==1?"323e5c4e7ef5ce9":"0"}, {"AllowOffscreenTarget","1"},
            {"EngineUIFunctionRVA",rule==0?"37f480":"0"}, {"EngineUIFunctionHash",rule==0?"533ebf8ba7423d58":"0"},
            {"EngineImageTimestamp",rule==0?"1730414432":"0"}, {"EngineImageSize",rule==0?"9019392":"0"},
            {"TraceEngineCallers","0"},
            {"SkipUnmatchedFrames",rule==2?"0":"1"}, {"TraceDraws",capture ? "1":"0"}, {"CaptureCandidates",capture ? "1":"0"}
        });
        Set(PathOf("AeonSR.ini"),"AeonSR",new Dictionary<string,string> { {"KeepInterface","0"}, {"SpatialJitter","0"}, {"UpscaleEffects","0"} });
    }
    internal static void ToggleInjection(bool enabled)
    {
        RequireStopped();
        string on=PathOf("dxgi.dll"), off=PathOf("dxgi.dll.kuro-disabled");
        string candidate=File.Exists(on) ? on:off;
        if (!File.Exists(candidate)) throw new FileNotFoundException("未找到本抗锯齿 Mod 的注入文件。");
        using (var hash=SHA256.Create()) using (var stream=File.OpenRead(candidate)) {
            string actual=BitConverter.ToString(hash.ComputeHash(stream)).Replace("-","");
            if (actual!="0CEE63F9C9F13F3AC909C5B4903F4DBB4B719A7AB3B4F13B0DEAF83C814B94F7")
                throw new IOException("这个注入文件不属于当前抗锯齿 Mod，未进行修改。");
        }
        if (enabled && File.Exists(off) && !File.Exists(on)) File.Move(off,on);
        else if (!enabled && File.Exists(on) && !File.Exists(off)) File.Move(on,off);
        else if ((enabled && File.Exists(on)) || (!enabled && File.Exists(off))) return;
        else throw new IOException("注入文件状态异常，请检查是否有其他同名文件。");
    }
}

internal static class Theme
{
    internal static readonly Color Background=Color.FromArgb(23,25,28), Surface=Color.FromArgb(36,39,43), Border=Color.FromArgb(65,70,77);
    internal static readonly Color Text=Color.FromArgb(238,241,244), Muted=Color.FromArgb(154,164,174), Accent=Color.FromArgb(66,203,161);
    internal static Label LabelOf(string text,int size=10,bool bold=false)
    { return new Label {Text=text,AutoSize=true,ForeColor=Text,Font=new Font("Microsoft YaHei UI",size,bold?FontStyle.Bold:FontStyle.Regular),Anchor=AnchorStyles.Left}; }
    internal static Button ButtonOf(string text,bool primary=false)
    {
        var button=new DarkButton {Text=text,Size=new Size(108,36),FlatStyle=FlatStyle.Flat,BackColor=primary?Accent:Surface,ForeColor=primary?Background:Text,Cursor=Cursors.Hand,UseVisualStyleBackColor=false};
        button.FlatAppearance.BorderColor=primary?Accent:Border; button.FlatAppearance.BorderSize=1;
        button.FlatAppearance.MouseOverBackColor=primary?Color.FromArgb(94,219,182):Color.FromArgb(48,53,58);
        return button;
    }
    internal static ComboBox Combo(params object[] items)
    {
        var combo=new DarkCombo {DropDownStyle=ComboBoxStyle.DropDownList,DrawMode=DrawMode.OwnerDrawFixed,ItemHeight=27,BackColor=Surface,ForeColor=Text,FlatStyle=FlatStyle.Flat,Dock=DockStyle.Fill};
        combo.Items.AddRange(items);
        combo.DrawItem+=delegate(object sender,DrawItemEventArgs e) {
            if(e.Index<0)return;
            bool selected=(e.State&DrawItemState.Selected)!=0;
            using(var brush=new SolidBrush(selected?Color.FromArgb(47,77,67):Surface))e.Graphics.FillRectangle(brush,e.Bounds);
            TextRenderer.DrawText(e.Graphics,combo.Items[e.Index].ToString(),combo.Font,new Rectangle(e.Bounds.X+10,e.Bounds.Y,e.Bounds.Width-16,e.Bounds.Height),combo.Enabled?Text:Muted,TextFormatFlags.Left|TextFormatFlags.VerticalCenter);
        };
        return combo;
    }
    internal static CheckBox Check(string text)
    { return new DarkCheck {Text=text,AutoSize=true,ForeColor=Text,BackColor=Background,Anchor=AnchorStyles.Left,Cursor=Cursors.Hand}; }
}

internal sealed class DarkCheck : CheckBox
{
    internal DarkCheck(){SetStyle(ControlStyles.UserPaint|ControlStyles.AllPaintingInWmPaint|ControlStyles.OptimizedDoubleBuffer,true);}
    public override Size GetPreferredSize(Size proposedSize){Size text=TextRenderer.MeasureText(Text,Font);return new Size(text.Width+28,Math.Max(24,text.Height+4));}
    protected override void OnPaint(PaintEventArgs e)
    {
        e.Graphics.Clear(BackColor);var box=new Rectangle(0,(Height-16)/2,16,16);
        using(var brush=new SolidBrush(Checked?(Enabled?Theme.Accent:Theme.Border):Theme.Surface))e.Graphics.FillRectangle(brush,box);
        using(var pen=new Pen(Enabled?Theme.Border:Theme.Muted))e.Graphics.DrawRectangle(pen,box);
        if(Checked)TextRenderer.DrawText(e.Graphics,"✓",Font,box,Theme.Background,TextFormatFlags.HorizontalCenter|TextFormatFlags.VerticalCenter|TextFormatFlags.NoPadding);
        TextRenderer.DrawText(e.Graphics,Text,Font,new Rectangle(25,0,Math.Max(0,Width-25),Height),Enabled?Theme.Text:Theme.Muted,TextFormatFlags.Left|TextFormatFlags.VerticalCenter|TextFormatFlags.SingleLine);
    }
}

internal sealed class SharpnessSlider : Control
{
    private int value;
    internal event EventHandler ValueChanged;
    internal int Value {get{return value;}set{int next=Math.Max(0,Math.Min(100,value));if(this.value==next)return;this.value=next;Invalidate();if(ValueChanged!=null)ValueChanged(this,EventArgs.Empty);}}
    internal SharpnessSlider(){SetStyle(ControlStyles.UserPaint|ControlStyles.AllPaintingInWmPaint|ControlStyles.OptimizedDoubleBuffer|ControlStyles.Selectable,true);Height=34;TabStop=true;Cursor=Cursors.Hand;BackColor=Theme.Background;}
    private void Position(int x){Value=(int)Math.Round(Math.Max(0,Math.Min(1,(x-8.0)/Math.Max(1,Width-16)))*100);}
    protected override void OnMouseDown(MouseEventArgs e){base.OnMouseDown(e);if(e.Button==MouseButtons.Left){Focus();Capture=true;Position(e.X);}}
    protected override void OnMouseMove(MouseEventArgs e){base.OnMouseMove(e);if(Capture)Position(e.X);}
    protected override void OnMouseUp(MouseEventArgs e){Capture=false;base.OnMouseUp(e);}
    protected override bool IsInputKey(Keys key){return key==Keys.Left||key==Keys.Right||key==Keys.Home||key==Keys.End||base.IsInputKey(key);}
    protected override void OnKeyDown(KeyEventArgs e){if(e.KeyCode==Keys.Left)Value--;else if(e.KeyCode==Keys.Right)Value++;else if(e.KeyCode==Keys.Home)Value=0;else if(e.KeyCode==Keys.End)Value=100;base.OnKeyDown(e);}
    protected override void OnPaint(PaintEventArgs e){e.Graphics.Clear(BackColor);int y=Height/2,x=8+(Width-16)*Value/100;using(var pen=new Pen(Theme.Border,4))e.Graphics.DrawLine(pen,8,y,Width-8,y);using(var pen=new Pen(Enabled?Theme.Accent:Theme.Muted,4))e.Graphics.DrawLine(pen,8,y,x,y);using(var brush=new SolidBrush(Enabled?Theme.Accent:Theme.Muted))e.Graphics.FillEllipse(brush,x-7,y-7,14,14);if(Focused)using(var pen=new Pen(Theme.Muted))e.Graphics.DrawRectangle(pen,0,0,Width-1,Height-1);}
}

internal sealed class CenteredNumberBox : TextBox
{
    [StructLayout(LayoutKind.Sequential)] private struct TextRect {public int Left,Top,Right,Bottom;}
    [DllImport("user32.dll")] private static extern IntPtr SendMessage(IntPtr window,int message,IntPtr wParam,ref TextRect rect);
    internal CenteredNumberBox(){Multiline=true;AutoSize=false;Height=34;TextAlign=HorizontalAlignment.Center;AcceptsReturn=false;WordWrap=false;}
    private void CenterText(){if(!IsHandleCreated)return;int line=TextRenderer.MeasureText("0.00",Font,Size.Empty,TextFormatFlags.NoPadding).Height;int top=Math.Max(0,(ClientSize.Height-line)/2+3);var rect=new TextRect{Left=3,Top=top,Right=ClientSize.Width-3,Bottom=Math.Min(ClientSize.Height,top+line)};SendMessage(Handle,0x00B4,IntPtr.Zero,ref rect);}
    protected override void OnHandleCreated(EventArgs e){base.OnHandleCreated(e);CenterText();}
    protected override void OnResize(EventArgs e){base.OnResize(e);CenterText();}
    protected override void OnFontChanged(EventArgs e){base.OnFontChanged(e);CenterText();}
    protected override void OnKeyPress(KeyPressEventArgs e){if(e.KeyChar=='\r'||e.KeyChar=='\n')e.Handled=true;base.OnKeyPress(e);}
}

internal sealed class DarkButton : Button
{
    private bool hover;
    protected override void OnMouseEnter(EventArgs e){hover=true;Invalidate();base.OnMouseEnter(e);}
    protected override void OnMouseLeave(EventArgs e){hover=false;Invalidate();base.OnMouseLeave(e);}
    protected override void OnPaint(PaintEventArgs e)
    {
        Color background=hover && Enabled?FlatAppearance.MouseOverBackColor:BackColor;
        using(var brush=new SolidBrush(background))e.Graphics.FillRectangle(brush,ClientRectangle);
        if(FlatAppearance.BorderSize>0)using(var pen=new Pen(Focused?Theme.Accent:FlatAppearance.BorderColor))e.Graphics.DrawRectangle(pen,0,0,Width-1,Height-1);
        TextRenderer.DrawText(e.Graphics,Text,Font,ClientRectangle,Enabled?ForeColor:Theme.Muted,TextFormatFlags.HorizontalCenter|TextFormatFlags.VerticalCenter|TextFormatFlags.SingleLine);
    }
}
internal sealed class DarkCombo : ComboBox
{
    [StructLayout(LayoutKind.Sequential)] private struct Rect {public int Left,Top,Right,Bottom;}
    [StructLayout(LayoutKind.Sequential)] private struct ComboInfo {public int Size;public Rect Item,Button;public int ButtonState;public IntPtr Combo,Edit,List;}
    [DllImport("user32.dll")] private static extern bool GetComboBoxInfo(IntPtr window,ref ComboInfo info);
    [DllImport("uxtheme.dll",CharSet=CharSet.Unicode)] private static extern int SetWindowTheme(IntPtr window,string application,string subId);
    [DllImport("user32.dll")] private static extern IntPtr SendMessage(IntPtr window,int message,IntPtr wParam,IntPtr lParam);
    private readonly PopupBackground popup=new PopupBackground();
    internal DarkCombo(){SetStyle(ControlStyles.UserPaint|ControlStyles.AllPaintingInWmPaint|ControlStyles.OptimizedDoubleBuffer,true);}
    protected override void OnHandleCreated(EventArgs e)
    {
        base.OnHandleCreated(e);SetWindowTheme(Handle,"","");
        var info=new ComboInfo{Size=Marshal.SizeOf(typeof(ComboInfo))};
        if(GetComboBoxInfo(Handle,ref info)&&info.List!=IntPtr.Zero){SetWindowTheme(info.List,"","");popup.AssignHandle(info.List);}
    }
    protected override void OnHandleDestroyed(EventArgs e){popup.ReleaseHandle();base.OnHandleDestroyed(e);}
    protected override void OnSelectedIndexChanged(EventArgs e){base.OnSelectedIndexChanged(e);Invalidate();}
    protected override void OnDropDownClosed(EventArgs e){base.OnDropDownClosed(e);Invalidate();}
    protected override void OnPaint(PaintEventArgs e)
    {
        e.Graphics.Clear(Theme.Surface);
        TextRenderer.DrawText(e.Graphics,Text,Font,new Rectangle(10,0,Math.Max(0,Width-38),Height),Enabled?Theme.Text:Theme.Muted,TextFormatFlags.Left|TextFormatFlags.VerticalCenter|TextFormatFlags.SingleLine|TextFormatFlags.EndEllipsis);
        using(var pen=new Pen(Focused?Theme.Accent:Theme.Border))e.Graphics.DrawRectangle(pen,0,0,Width-1,Height-1);
        TextRenderer.DrawText(e.Graphics,"▾",Font,new Rectangle(Width-24,0,20,Height),Enabled?Theme.Text:Theme.Muted,TextFormatFlags.HorizontalCenter|TextFormatFlags.VerticalCenter);
    }
    internal void VerifyPopupBackground()
    {
        CreateControl();var window=Handle;
        if(popup.Handle==IntPtr.Zero)throw new Exception("下拉列表窗口初始化失败。");
        using(var bitmap=new Bitmap(32,32))using(var graphics=Graphics.FromImage(bitmap)){
            graphics.Clear(Color.White);var dc=graphics.GetHdc();
            try{SendMessage(popup.Handle,0x0014,dc,IntPtr.Zero);}finally{graphics.ReleaseHdc(dc);}
            if(bitmap.GetPixel(16,16).ToArgb()!=Theme.Surface.ToArgb())throw new Exception("下拉列表深色背景测试失败。");
        }
    }
    protected override void WndProc(ref Message m)
    {
        if(m.Msg==0x0014){m.Result=new IntPtr(1);return;}
        base.WndProc(ref m);
    }
    private sealed class PopupBackground : NativeWindow
    {
        protected override void WndProc(ref Message m)
        {
            if(m.Msg==0x0014){if(m.WParam!=IntPtr.Zero)using(var graphics=Graphics.FromHdc(m.WParam))graphics.Clear(Theme.Surface);m.Result=new IntPtr(1);return;}
            base.WndProc(ref m);
        }
    }
}

internal class DarkForm : Form
{
    [DllImport("dwmapi.dll")] private static extern int DwmSetWindowAttribute(IntPtr window,int attribute,ref int value,int size);
    internal DarkForm() { BackColor=Theme.Background; ForeColor=Theme.Text; Font=new Font("Microsoft YaHei UI",10); AutoScaleMode=AutoScaleMode.Dpi; }
    protected override void OnHandleCreated(EventArgs e) { base.OnHandleCreated(e); try {int dark=1;DwmSetWindowAttribute(Handle,20,ref dark,4);}catch(DllNotFoundException){} }
    internal static void Error(IWin32Window owner,string text)
    {
        using(var dialog=new DarkForm {Text="无法完成操作",ClientSize=new Size(480,190),FormBorderStyle=FormBorderStyle.FixedDialog,MaximizeBox=false,MinimizeBox=false,StartPosition=FormStartPosition.CenterParent}) {
            var message=new Label {Text=text,Dock=DockStyle.Fill,Padding=new Padding(22),ForeColor=Theme.Text};
            var footer=new Panel {Dock=DockStyle.Bottom,Height=62,Padding=new Padding(20,10,20,16)};
            var close=Theme.ButtonOf("确定",true); close.Dock=DockStyle.Right; close.DialogResult=DialogResult.OK;
            footer.Controls.Add(close); dialog.Controls.Add(message);dialog.Controls.Add(footer);dialog.AcceptButton=close;dialog.ShowDialog(owner);
        }
    }
}

internal sealed class SettingsWindow : DarkForm
{
    private readonly ComboBox backend=Theme.Combo("NVIDIA DLAA / DLSS","AMD FSR","Intel XeSS","关闭抗锯齿效果","按游戏显卡自动选择");
    private readonly ComboBox quality=Theme.Combo("DLAA","质量","均衡","性能","超级性能");
    private int[] qualityIds=new[]{0,2,3,4,5};
    private int qualityBackend=-1;
    private readonly ComboBox motionQuality=Theme.Combo("Balanced","High");
    private readonly ComboBox rule=Theme.Combo("引擎边界（推荐）","Shader 签名（0.3.1）","无规则（全屏 AA）");
    private readonly CheckBox capture=Theme.Check("采集绘制目标（诊断）");
    private readonly SharpnessSlider sharp=new SharpnessSlider();
    private readonly TextBox sharpValue=new CenteredNumberBox{Text="0.00",BackColor=Theme.Surface,ForeColor=Theme.Text,BorderStyle=BorderStyle.FixedSingle};
    private readonly Label status=Theme.LabelOf(""), sceneStatus=Theme.LabelOf("");
    private readonly Panel aaPage=new Panel(), scenePage=new Panel();
    private readonly Button aaTab=Theme.ButtonOf("抗锯齿"), uiTab=Theme.ButtonOf("场景与界面"), toggle=Theme.ButtonOf("关闭注入");
    private readonly ToolTip tips=new ToolTip();
    internal SettingsWindow()
    {
        Text="黎之轨迹抗锯齿 Mod 设置（验证版）";ClientSize=new Size(640,460);MinimumSize=new Size(656,499);StartPosition=FormStartPosition.CenterScreen;
        var root=new TableLayoutPanel {Dock=DockStyle.Fill,Padding=new Padding(26,22,26,20),ColumnCount=1,RowCount=5};
        root.RowStyles.Add(new RowStyle(SizeType.Absolute,55));root.RowStyles.Add(new RowStyle(SizeType.Absolute,48));root.RowStyles.Add(new RowStyle(SizeType.Percent,100));root.RowStyles.Add(new RowStyle(SizeType.Absolute,52));root.RowStyles.Add(new RowStyle(SizeType.Absolute,28));Controls.Add(root);
        var header=new TableLayoutPanel {Dock=DockStyle.Fill,ColumnCount=2};header.ColumnStyles.Add(new ColumnStyle(SizeType.Percent,100));header.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute,65));
        header.Controls.Add(Theme.LabelOf("黎之轨迹抗锯齿",18,true),0,0);var version=Theme.LabelOf("验证版");version.ForeColor=Theme.Muted;header.Controls.Add(version,1,0);root.Controls.Add(header,0,0);
        var tabs=new FlowLayoutPanel {Dock=DockStyle.Fill,WrapContents=false};aaTab.Width=145;uiTab.Width=145;aaTab.FlatAppearance.BorderSize=0;uiTab.FlatAppearance.BorderSize=0;tabs.Controls.AddRange(new Control[]{aaTab,uiTab});root.Controls.Add(tabs,0,1);
        var pages=new Panel {Dock=DockStyle.Fill};aaPage.Dock=scenePage.Dock=DockStyle.Fill;pages.Controls.Add(aaPage);pages.Controls.Add(scenePage);root.Controls.Add(pages,0,2);
        var fields=Grid();aaPage.Controls.Add(fields);
        var sharpRow=new TableLayoutPanel{Dock=DockStyle.Fill,ColumnCount=2,RowCount=1};sharpRow.ColumnStyles.Add(new ColumnStyle(SizeType.Percent,100));sharpRow.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute,65));sharp.Dock=DockStyle.Fill;sharpRow.Controls.Add(sharp,0,0);sharpValue.Anchor=AnchorStyles.Left|AnchorStyles.Right;sharpRow.Controls.Add(sharpValue,1,0);sharp.ValueChanged+=delegate{sharpValue.Text=(sharp.Value/100.0).ToString("0.00",System.Globalization.CultureInfo.InvariantCulture);};
        sharpValue.Leave+=delegate{decimal number;if(decimal.TryParse(sharpValue.Text,System.Globalization.NumberStyles.Float,System.Globalization.CultureInfo.InvariantCulture,out number)&&number>=0&&number<=1)sharp.Value=(int)Math.Round(number*100);};
        Row(fields,0,"抗锯齿算法",backend);Row(fields,1,"性能档位",quality);Row(fields,2,"运动估计质量",motionQuality);Row(fields,3,"锐化强度",sharpRow);Row(fields,4,"AA 规则",rule);
        var sceneFields=Grid();scenePage.Controls.Add(sceneFields);Row(sceneFields,0,"当前规则",sceneStatus);Row(sceneFields,1,"诊断采集",capture);Row(sceneFields,2,"游戏适配",Theme.LabelOf("云豹版 DX11 · 16257982"));Row(sceneFields,3,"生效时间",Theme.LabelOf("保存后重新启动游戏"));
        var actions=new FlowLayoutPanel {Dock=DockStyle.Fill,WrapContents=false,Padding=new Padding(0,6,0,0)};
        var save=Theme.ButtonOf("保存设置",true);var launch=Theme.ButtonOf("启动游戏");var logs=Theme.ButtonOf("查看日志");actions.Controls.AddRange(new Control[]{save,launch,toggle,logs});root.Controls.Add(actions,0,3);root.Controls.Add(status,0,4);
        aaTab.Click+=delegate{ShowTab(false);};uiTab.Click+=delegate{ShowTab(true);};backend.SelectedIndexChanged+=delegate{RefreshControls();};rule.SelectedIndexChanged+=delegate{RefreshControls();};
        save.Click+=delegate{Run(delegate{Config.RequireStopped();decimal number;if(!decimal.TryParse(sharpValue.Text,System.Globalization.NumberStyles.Float,System.Globalization.CultureInfo.InvariantCulture,out number)||number<0||number>1)throw new ArgumentException("锐化强度可填写 0.00 到 1.00，默认 0；需要时推荐 0.03 到 0.08。");sharp.Value=(int)Math.Round(number*100);Config.SetRule(rule.SelectedIndex,capture.Checked);Config.SelectBackend(backend.SelectedIndex,qualityIds[Math.Max(0,quality.SelectedIndex)],sharp.Value/100.0f,motionQuality.SelectedIndex);status.Text="设置已保存，下次启动游戏时生效。";status.ForeColor=Theme.Accent;});};
        toggle.Click+=delegate{Run(delegate{Config.ToggleInjection(!File.Exists(Config.PathOf("dxgi.dll")));RefreshStatus();});};
        launch.Click+=delegate{Run(delegate{if(!File.Exists(Config.PathOf("ed9.exe")))throw new FileNotFoundException("未找到 ed9.exe，请把设置器放在游戏目录中。");Process.Start(new ProcessStartInfo(Config.PathOf("ed9.exe")){WorkingDirectory=Config.Root,UseShellExecute=true});});};
        logs.Click+=delegate{Run(delegate{string path=Config.PathOf("KuroUI.log");if(!File.Exists(path))path=Config.PathOf("AeonSR.log");if(!File.Exists(path))throw new FileNotFoundException("还没有运行日志，请先启动一次游戏。");Process.Start(path);});};
        tips.SetToolTip(capture,"仅用于诊断，GPU 回读会造成卡顿。");tips.SetToolTip(toggle,"只切换本抗锯齿 Mod 的注入文件。");
        tips.SetToolTip(sharpValue,"可填写 0.00–1.00；默认 0；需要时推荐 0.03–0.08。");
        tips.SetToolTip(rule,"无规则会处理最终画面，UI 文字也参与 AA，可能变软或残影。");
        LoadSettings();ShowTab(false);RefreshControls();RefreshStatus();
    }
    private static TableLayoutPanel Grid()
    {
        var grid=new TableLayoutPanel {Dock=DockStyle.Fill,ColumnCount=2,RowCount=6,Padding=new Padding(0,18,0,0)};grid.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute,145));grid.ColumnStyles.Add(new ColumnStyle(SizeType.Percent,100));
        for(int i=0;i<6;i++)grid.RowStyles.Add(new RowStyle(SizeType.Absolute,40));return grid;
    }
    private static void Row(TableLayoutPanel grid,int row,string name,Control control) {var label=Theme.LabelOf(name);label.ForeColor=Theme.Muted;grid.Controls.Add(label,0,row);grid.Controls.Add(control,1,row);}
    private void LoadSettings()
    {
        var a=Config.Read(Config.PathOf("AeonSR.ini"));var s=Config.Read(Config.PathOf("KuroUI.ini"));
        int b=4,q=0,flow=1;if(a.ContainsKey("Upscaler")&&!int.TryParse(a["Upscaler"],out b))b=4;if(a.ContainsKey("UpscaleMode"))int.TryParse(a["UpscaleMode"],out q);if(a.ContainsKey("InternalFlowQuality"))int.TryParse(a["InternalFlowQuality"],out flow);
        backend.SelectedIndex=!Config.Flag(a,"Enabled",true)||b==3?3:b>=0&&b<3?b:4;int qualityIndex=Array.IndexOf(qualityIds,q);quality.SelectedIndex=qualityIndex>=0?qualityIndex:q==1&&qualityIds.Length>1?1:0;motionQuality.SelectedIndex=Math.Max(0,Math.Min(flow,1));
        capture.Checked=Config.Flag(s,"CaptureCandidates",false);
        rule.SelectedIndex=Config.ReadRule(s);
        decimal value=0m;if(a.ContainsKey("Sharpness"))decimal.TryParse(a["Sharpness"],System.Globalization.NumberStyles.Float,System.Globalization.CultureInfo.InvariantCulture,out value);sharp.Value=(int)(Math.Max(0,Math.Min(1.00m,value))*100);
    }
    private void RefreshControls()
    {
        if(qualityBackend!=backend.SelectedIndex){int mode=quality.SelectedIndex>=0&&quality.SelectedIndex<qualityIds.Length?qualityIds[quality.SelectedIndex]:0;qualityBackend=backend.SelectedIndex;quality.Items.Clear();
            if(qualityBackend==0){qualityIds=new[]{0,2,3,4,5};quality.Items.AddRange(new object[]{"DLAA","质量","均衡","性能","超级性能"});}
            else if(qualityBackend==1){qualityIds=new[]{0,2,3,4,5};quality.Items.AddRange(new object[]{"Native AA","质量","均衡","性能","超级性能"});}
            else if(qualityBackend==2){qualityIds=new[]{0,1,2,3,4,5};quality.Items.AddRange(new object[]{"Native AA","Ultra Quality","质量","均衡","性能","超级性能"});}
            else{qualityIds=new[]{0};quality.Items.Add(qualityBackend==4?"Native AA（自动）":"不适用");}
            int index=Array.IndexOf(qualityIds,mode);quality.SelectedIndex=index>=0?index:mode==1&&qualityIds.Length>1?1:0;
        }
        bool vendor=backend.SelectedIndex>=0 && backend.SelectedIndex!=3;quality.Enabled=vendor;motionQuality.Enabled=vendor;sharp.Enabled=sharpValue.Enabled=vendor;
        rule.Enabled=vendor;sceneStatus.Text=rule.SelectedIndex==1?"0.3.1 shader 边界":rule.SelectedIndex==2?"全屏模式：UI 也参与 AA":"引擎 UI 边界保护";
        sceneStatus.ForeColor=rule.SelectedIndex==2?Color.FromArgb(241,183,83):Theme.Accent;
    }
    private void RefreshStatus()
    {
        bool on=File.Exists(Config.PathOf("dxgi.dll")),off=File.Exists(Config.PathOf("dxgi.dll.kuro-disabled"));
        status.Text=on?"抗锯齿 Mod 注入已开启":off?"抗锯齿 Mod 注入已关闭":"未找到抗锯齿 Mod 文件";
        status.ForeColor=on?Theme.Accent:Theme.Muted;toggle.Text=on?"关闭注入":"开启注入";toggle.Enabled=on||off;
    }
    private void ShowTab(bool ui)
    {
        aaPage.Visible=!ui;scenePage.Visible=ui;aaTab.ForeColor=ui?Theme.Muted:Theme.Accent;uiTab.ForeColor=ui?Theme.Accent:Theme.Muted;
        aaTab.BackColor=ui?Theme.Background:Theme.Surface;uiTab.BackColor=ui?Theme.Surface:Theme.Background;
    }
    internal void SelectSceneTab() {ShowTab(true);}
    internal void VerifyControls()
    {
        ((DarkCombo)backend).VerifyPopupBackground();((DarkCombo)quality).VerifyPopupBackground();((DarkCombo)motionQuality).VerifyPopupBackground();((DarkCombo)rule).VerifyPopupBackground();
        for(int b=0;b<3;b++){
            backend.SelectedIndex=b;
            string native=b==0?"DLAA":"Native AA";
            if(quality.Items[0].ToString()!=native || qualityIds[0]!=0)throw new Exception("原生档位映射测试失败。");
            int expected=b==2?1:2;if(qualityIds[1]!=expected)throw new Exception("质量档位映射测试失败。");
        }
        sharp.Value=100;if(sharp.Value!=100 || sharpValue.Text!="1.00")throw new Exception("锐化上限测试失败。");
        sharp.Value=0;if(sharp.Value!=0 || sharpValue.Text!="0.00")throw new Exception("锐化关闭测试失败。");
        backend.SelectedIndex=3;RefreshControls();if(motionQuality.Enabled||sharp.Enabled)throw new Exception("关闭状态测试失败。");
        backend.SelectedIndex=4;RefreshControls();if(!motionQuality.Enabled||qualityIds[0]!=0)throw new Exception("自动选择测试失败。");
        for(int i=0;i<3;i++){rule.SelectedIndex=i;RefreshControls();if(!rule.Enabled || (i==2 && sceneStatus.ForeColor==Theme.Accent))throw new Exception("AA 规则控件测试失败。");}
        if(Config.ReadRule(new Dictionary<string,string>())!=0)throw new Exception("默认规则测试失败。");
    }
    private void Run(Action action) {try{action();}catch(Exception e){DarkForm.Error(this,e.Message);}}
}

internal static class Program
{
    [STAThread] private static int Main(string[] args)
    {
        try {
            if(args.Length==2 && (args[0]=="--render-test" || args[0]=="--render-scene-test")) {
                Application.EnableVisualStyles();using(var form=new SettingsWindow()) {
                    if(args[0]=="--render-scene-test")form.SelectSceneTab();form.StartPosition=FormStartPosition.Manual;form.Location=new Point(-10000,-10000);form.ShowInTaskbar=false;form.Show();Application.DoEvents();form.PerformLayout();
                    using(var bitmap=new Bitmap(form.Width,form.Height)){form.DrawToBitmap(bitmap,new Rectangle(0,0,bitmap.Width,bitmap.Height));bitmap.Save(args[1],System.Drawing.Imaging.ImageFormat.Png);}form.Close();
                }return 0;
            }
            if(args.Length==1 && args[0]=="--self-test") {
                using(var window=new SettingsWindow())window.VerifyControls();
                string path=Config.PathOf("settings-self-test.ini");if(File.Exists(path))throw new IOException("自测临时文件已存在。");
                try{File.WriteAllText(path,"[GENERAL]\nKeep=42\n[AeonSR]\nEnabled=0\n");Config.Set(path,"AeonSR",new Dictionary<string,string>{{"Enabled","1"},{"Upscaler","2"}});var values=Config.Read(path);if(values["Keep"]!="42" || values["Enabled"]!="1" || values["Upscaler"]!="2")throw new Exception("配置保留测试失败。");}
                finally{if(File.Exists(path))File.Delete(path);}Console.WriteLine("配置保留测试通过。");return 0;
            }
            if(args.Length==2 && args[0]=="--backend") {int b=Array.IndexOf(new[]{"dlss","fsr","xess","off","auto"},args[1].ToLowerInvariant());Config.SelectBackend(b,0,0f);return 0;}
            if(args.Length==2 && args[0]=="--rule"){Config.SetRule(Array.IndexOf(new[]{"engine","shader","full"},args[1].ToLowerInvariant()),false);return 0;}
            if(args.Length==1 && (args[0]=="--disable" || args[0]=="--enable")){Config.ToggleInjection(args[0]=="--enable");return 0;}
            if(args.Length!=0)throw new ArgumentException("无法识别启动参数。");
            Application.EnableVisualStyles();Application.SetCompatibleTextRenderingDefault(false);Application.Run(new SettingsWindow());return 0;
        }catch(Exception e){Console.Error.WriteLine(e.Message);return 1;}
    }
}
