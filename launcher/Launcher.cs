using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Security.Cryptography;
using System.Text;
using System.Threading;
using System.Threading.Tasks;
using System.Windows.Forms;

internal static class Launcher {
    internal const string Version = "0.2-alpha";
    internal const string XexHash = "0be3f2e797a31df5d2a231635e4dc5520da36d4b9dbc7b995cf1bd3b0fedb86b";
    internal static readonly string Root = AppDomain.CurrentDomain.BaseDirectory;
    internal static string Game { get { return Path.Combine(Root, "game-data"); } }
    internal static string Hash(string path) {
        using (var f = File.OpenRead(path)) using (var sha = SHA256.Create())
            return BitConverter.ToString(sha.ComputeHash(f)).Replace("-", "").ToLowerInvariant();
    }
    internal static void Validate(string directory) {
        string xex = Path.Combine(directory, "default.xex");
        if (!File.Exists(xex) || Hash(xex) != XexHash)
            throw new InvalidDataException("This build requires the original USA Xbox 360 release of Dynasty Warriors: Gundam 2. The selected default.xex does not match.");
        if (!Directory.Exists(Path.Combine(directory,"data")) || !Directory.Exists(Path.Combine(directory,"Res")))
            throw new InvalidDataException("The game data and Res folders must be present beside default.xex.");
        string[] archives = {"LINKDATA.ANS", "LINKDATA.BNS", "LINKDATA.CNS"};
        long[] lengths = {645439488L, 1591506944L, 2071525376L};
        for (int i=0; i<archives.Length; i++) {
            var archive = new FileInfo(Path.Combine(directory,"data",archives[i]));
            if(!archive.Exists || archive.Length != lengths[i])
                throw new InvalidDataException("The game archive is missing or incomplete: " + archives[i]);
        }
    }
    internal static string Quote(string value) {
        var b = new StringBuilder("\""); int slashes = 0;
        foreach (char c in value) {
            if (c == '\\') { slashes++; continue; }
            if (c == '"') b.Append('\\', slashes * 2 + 1).Append(c);
            else b.Append('\\', slashes).Append(c);
            slashes = 0;
        }
        return b.Append('\\', slashes * 2).Append('"').ToString();
    }
    internal static int Scale() {
        int n; string file = Path.Combine(Root,"display.ini");
        return File.Exists(file) && int.TryParse(File.ReadAllText(file).Trim(), out n) && n >= 1 && n <= 3 ? n : 1;
    }
    internal static string Arguments() {
        string id = DateTime.UtcNow.ToString("yyyyMMdd-HHmmss") + "-" + Process.GetCurrentProcess().Id;
        int n = Scale();
        return "--game_data_root=" + Quote(Game) + " --user_data_root=" + Quote(Path.Combine(Root,"user-data"))
            + " --cache_root=" + Quote(Path.Combine(Root,"cache"))
            + " --log_file=" + Quote(Path.Combine(Root,"logs","dwg2-"+id+".log"))
            + " --log_level=info --gpu_plugin=xenos --input_backend=sdl --mnk_mode=true"
            + " --fullscreen=false --dwg2_present_fps=0 --dwg2_host_vsync=false --dwg2_skip_startup_logos=true"
            + " --resolution_scale=" + n + " --draw_resolution_scale_x=" + n + " --draw_resolution_scale_y=" + n
            + " --window_width=" + (1280*n) + " --window_height=" + (720*n);
    }
    internal static void CopyFolder(string source, string target, Action<string> report, CancellationToken token) {
        if ((File.GetAttributes(source) & FileAttributes.ReparsePoint) != 0)
            throw new IOException("Linked folders are not accepted during import.");
        Directory.CreateDirectory(target);
        foreach (string file in Directory.GetFiles(source)) {
            token.ThrowIfCancellationRequested();
            if ((File.GetAttributes(file) & FileAttributes.ReparsePoint) != 0) throw new IOException("Linked files are not accepted.");
            report("Importing " + Path.GetFileName(file));
            using (var input = File.OpenRead(file)) using (var output = new FileStream(Path.Combine(target,Path.GetFileName(file)),FileMode.CreateNew))
                Copy(input,output,input.Length,token);
        }
        foreach (string dir in Directory.GetDirectories(source)) {
            if (Path.GetFileName(dir).Equals("$SystemUpdate",StringComparison.OrdinalIgnoreCase)) continue;
            CopyFolder(dir,Path.Combine(target,Path.GetFileName(dir)),report,token);
        }
    }
    internal static void Copy(Stream input, Stream output, long bytes, CancellationToken token) {
        byte[] buffer = new byte[1024*1024];
        while (bytes > 0) {
            token.ThrowIfCancellationRequested();
            int count = input.Read(buffer,0,(int)Math.Min(bytes,buffer.Length));
            if (count == 0) throw new EndOfStreamException("The game image is truncated.");
            output.Write(buffer,0,count); bytes -= count;
        }
    }
    internal static void Import(string source, Action<string> report, CancellationToken token) {
        if (Directory.Exists(Game)) throw new IOException("game-data already exists. Your existing installation has been preserved.");
        string staging = Path.Combine(Root,"game-data.import-"+Guid.NewGuid().ToString("N"));
        Directory.CreateDirectory(staging);
        try {
            if (Path.GetExtension(source).Equals(".iso",StringComparison.OrdinalIgnoreCase)) {
                using (var disc = new Disc(source)) disc.Extract(staging,report,token);
            } else {
                if (!Path.GetFileName(source).Equals("default.xex",StringComparison.OrdinalIgnoreCase))
                    throw new InvalidDataException("Choose an ISO or default.xex with its original game folders.");
                string from = Path.GetFullPath(Path.GetDirectoryName(source)).TrimEnd('\\')+"\\";
                if (Path.GetFullPath(staging).StartsWith(from,StringComparison.OrdinalIgnoreCase))
                    throw new IOException("Choose the original extracted game folder outside this launcher folder.");
                Validate(from); CopyFolder(from,staging,report,token);
            }
            Validate(staging);
            token.ThrowIfCancellationRequested();
            Directory.Move(staging,Game);
        } finally { if (Directory.Exists(staging)) Directory.Delete(staging,true); }
    }
    internal static void EnsureExecutable(Action<string> report) {
        string exe = Path.Combine(Root,"bin","dwg2.exe");
        if (!File.Exists(exe)) {
            string build = Path.Combine(Root,"scripts","build-game.ps1");
            if (!File.Exists(build)) throw new FileNotFoundException("The build recipe is missing. Keep the launcher beside scripts and src.");
            report("Building your local game executable. This may take several minutes...");
            var info = new ProcessStartInfo("powershell.exe", "-NoProfile -ExecutionPolicy Bypass -File " + Quote(build) + " -GameData " + Quote(Game) + " -Output " + Quote(Path.Combine(Root,"bin"))) {
                UseShellExecute=false, CreateNoWindow=true, WorkingDirectory=Root,
                RedirectStandardError=true, RedirectStandardOutput=true
            };
            using (var log = new StreamWriter(Path.Combine(Root,"logs","first-build.log"),false,Encoding.UTF8))
            using (var p = new Process()) {
                object gate = new object(); p.StartInfo=info;
                DataReceivedEventHandler record=delegate(object sender,DataReceivedEventArgs e) { if(e.Data!=null) lock(gate) { log.WriteLine(e.Data); log.Flush(); } };
                p.OutputDataReceived+=record; p.ErrorDataReceived+=record;
                p.Start(); p.BeginOutputReadLine(); p.BeginErrorReadLine(); p.WaitForExit();
                if(p.ExitCode!=0) throw new InvalidOperationException("The local build failed. Your import is saved; see logs/first-build.log and BUILDING.md for the required build tools.");
            }
        }
        foreach (string file in new string[]{"dwg2.exe","rexruntime.dll","rexgpu-xenos.dll","TracyClient.dll"})
            if(!File.Exists(Path.Combine(Root,"bin",file))) throw new FileNotFoundException("Missing runtime file: "+file);
        File.WriteAllText(Path.Combine(Root,"installed.txt"),Version+"\n"+XexHash+"\n");
    }
    [STAThread] private static int Main(string[] args) {
        Application.EnableVisualStyles(); Application.SetCompatibleTextRenderingDefault(false);
        bool dry=false, validate=false, noLaunch=false; string import=null;
        try {
            for(int i=0;i<args.Length;i++) {
                if(args[i]=="--dry-run") dry=true;
                else if(args[i]=="--validate") validate=true;
                else if(args[i]=="--no-launch") noLaunch=true;
                else if(args[i]=="--import" && i+1<args.Length) import=args[++i];
                else throw new ArgumentException("Unknown option: "+args[i]);
            }
            if(dry) { File.WriteAllText(Path.Combine(Root,"launch-preview.txt"),Arguments()); return 0; }
            bool created;
            using(var mutex=new Mutex(true,"Local\\DWG2-Launcher-"+BitConverter.ToString(SHA256.Create().ComputeHash(Encoding.UTF8.GetBytes(Path.GetFullPath(Root).ToLowerInvariant()))).Replace("-",""),out created)) {
                if(!created) throw new InvalidOperationException("The launcher or game is already running from this folder.");
                Directory.CreateDirectory(Path.Combine(Root,"logs"));
                if(validate) { Validate(Game); return 0; }
                if(!Directory.Exists(Game)) {
                    if(import==null) using(var picker=new OpenFileDialog()) {
                        picker.Title="Import your Dynasty Warriors: Gundam 2 game";
                        picker.Filter="Xbox 360 game image|*.iso;default.xex|ISO image|*.iso|Extracted game|default.xex";
                        if(picker.ShowDialog()!=DialogResult.OK) return 0;
                        import=picker.FileName;
                    }
                    Progress.Run(delegate(Action<string> report,CancellationToken token) { Import(import,report,token); },true);
                }
                Validate(Game);
                if(File.Exists(Path.Combine(Root,"bin","dwg2.exe"))) EnsureExecutable(delegate(string text){});
                else Progress.Run(delegate(Action<string> report,CancellationToken token) { EnsureExecutable(report); },false);
                if(noLaunch) return 0;
                var info=new ProcessStartInfo(Path.Combine(Root,"bin","dwg2.exe"),Arguments()) {
                    UseShellExecute=false,WorkingDirectory=Path.Combine(Root,"bin"),CreateNoWindow=true,
                    RedirectStandardOutput=true,RedirectStandardError=true
                };
                using(var log=new StreamWriter(Path.Combine(Root,"logs","last-console.log"),false,Encoding.UTF8))
                using(var p=new Process()) {
                    object gate=new object();p.StartInfo=info;
                    DataReceivedEventHandler record=delegate(object sender,DataReceivedEventArgs e) { if(e.Data!=null) lock(gate) { log.WriteLine(e.Data);log.Flush(); } };
                    p.OutputDataReceived+=record;p.ErrorDataReceived+=record;
                    p.Start();p.BeginOutputReadLine();p.BeginErrorReadLine();p.WaitForExit();
                    if(p.ExitCode!=0) throw new InvalidOperationException("The game exited unexpectedly ("+p.ExitCode+"). See the logs folder.");
                }
            }
            return 0;
        } catch(OperationCanceledException) { return 0; }
        catch(Exception e) { File.WriteAllText(Path.Combine(Root,"launcher-error.txt"),e.ToString());if(!validate && !noLaunch) MessageBox.Show(e.Message,"DWG2 Launcher",MessageBoxButtons.OK,MessageBoxIcon.Error);return 1; }
    }
}

internal sealed class Progress : Form {
    readonly Label label=new Label { Dock=DockStyle.Fill,Padding=new Padding(18),Text="Preparing..." };
    readonly CancellationTokenSource cancel=new CancellationTokenSource();
    Exception failure; bool done;
    private Progress(Action<Action<string>,CancellationToken> action,bool cancellable) {
        Text="DWG2 Setup"; Width=520;Height=150;StartPosition=FormStartPosition.CenterScreen;
        FormBorderStyle=FormBorderStyle.FixedDialog;MaximizeBox=false;MinimizeBox=false;ControlBox=cancellable;
        Controls.Add(label); Controls.Add(new ProgressBar { Dock=DockStyle.Bottom,Style=ProgressBarStyle.Marquee });
        FormClosing+=delegate(object sender,FormClosingEventArgs e) { if(!done) { cancel.Cancel();e.Cancel=true;label.Text="Cancelling safely..."; } };
        Shown+=async delegate {
            try { await Task.Run(delegate { action(delegate(string text) { BeginInvoke((Action)delegate { label.Text=text; }); },cancel.Token); }); }
            catch(Exception e) { failure=e; }
            done=true;Close();
        };
    }
    internal static void Run(Action<Action<string>,CancellationToken> action,bool cancellable) {
        using(var form=new Progress(action,cancellable)) { form.ShowDialog();if(form.failure!=null) throw form.failure; }
    }
}

// Bounded read-only XDVDFS reader. Full-disc and game-partition Xbox 360 images.
internal sealed class Disc : IDisposable {
    readonly FileStream file; readonly BinaryReader reader; long partition;
    uint rootSector,rootLength; int files;long total;
    readonly HashSet<long> directories=new HashSet<long>();
    internal Disc(string path) {
        file=new FileStream(path,FileMode.Open,FileAccess.Read,FileShare.Read);reader=new BinaryReader(file);
        try {
            bool found=false;
            foreach(long offset in new long[]{0,0xFD90000,0x2080000,0x18300000}) {
                if(offset+0x10000+36>file.Length) continue;
                file.Position=offset+0x10000;
                if(Encoding.ASCII.GetString(reader.ReadBytes(20))!="MICROSOFT*XBOX*MEDIA") continue;
                partition=offset;rootSector=reader.ReadUInt32();rootLength=reader.ReadUInt32();found=true;break;
            }
            if(!found) throw new InvalidDataException("No supported Xbox game filesystem was found in this ISO.");
        } catch { Dispose();throw; }
    }
    byte[] Read(long offset,int count) {
        if(offset<0 || count<0 || offset>file.Length-count) throw new InvalidDataException("The ISO contains an out-of-range entry.");
        file.Position=offset;byte[] data=reader.ReadBytes(count);if(data.Length!=count) throw new EndOfStreamException();return data;
    }
    internal void Extract(string destination,Action<string> report,CancellationToken token) { DirectoryTree(rootSector,rootLength,destination,0,report,token); }
    void DirectoryTree(uint sector,uint length,string destination,int depth,Action<string> report,CancellationToken token) {
        if(length==0) return;
        long offset=partition+(long)sector*2048;
        if(depth>32 || length>16*1024*1024 || !directories.Add(offset)) throw new InvalidDataException("Invalid directory tree in ISO.");
        byte[] table=Read(offset,(int)length);var pending=new Stack<int>();var visited=new HashSet<int>();pending.Push(0);
        Directory.CreateDirectory(destination);
        while(pending.Count>0) {
            token.ThrowIfCancellationRequested();int pos=pending.Pop();
            if(!visited.Add(pos) || pos<0 || pos+14>table.Length) throw new InvalidDataException("Invalid directory node.");
            ushort left=BitConverter.ToUInt16(table,pos),right=BitConverter.ToUInt16(table,pos+2);
            uint entrySector=BitConverter.ToUInt32(table,pos+4),size=BitConverter.ToUInt32(table,pos+8);
            int attr=table[pos+12],nameLength=table[pos+13];
            if(nameLength==0 || pos+14+nameLength>table.Length) throw new InvalidDataException("Invalid ISO filename.");
            string name=Encoding.ASCII.GetString(table,pos+14,nameLength);
            if(name=="." || name==".." || name.IndexOfAny(Path.GetInvalidFileNameChars())>=0 || name.EndsWith(".") || name.EndsWith(" ")) throw new InvalidDataException("Unsafe ISO filename.");
            string target=Path.Combine(destination,name);
            if(left!=0) pending.Push(left*4);if(right!=0) pending.Push(right*4);
            if(name.Equals("$SystemUpdate",StringComparison.OrdinalIgnoreCase)) continue;
            if((attr&0x10)!=0) DirectoryTree(entrySector,size,target,depth+1,report,token);
            else {
                if(++files>100000 || (total+=size)>32L*1024*1024*1024) throw new InvalidDataException("Unreasonable ISO content size.");
                long start=partition+(long)entrySector*2048;
                if(start>file.Length-size) throw new InvalidDataException("Truncated file in ISO.");
                report("Importing "+name);file.Position=start;
                using(var output=new FileStream(target,FileMode.CreateNew,FileAccess.Write)) { Launcher.Copy(file,output,size,token);output.Flush(true); }
            }
        }
    }
    public void Dispose() { reader.Dispose();file.Dispose(); }
}
