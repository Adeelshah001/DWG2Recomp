using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Drawing;
using System.IO;
using System.IO.Compression;
using System.Net;
using System.Runtime.InteropServices;
using System.Security.Cryptography;
using System.Text;
using System.Text.RegularExpressions;
using System.Threading;
using System.Web.Script.Serialization;
using System.Windows.Forms;

// Original launcher updater. Network/disk work never runs on the game thread.
internal sealed class UpdateService : IDisposable {
    internal const string Version="0.3-alpha";
    internal const string EventVariable="DWG2_UPDATE_EVENT";
    internal const string Repository="Adeelshah001/DWG2Recomp";
    internal const string Releases="https://api.github.com/repos/"+Repository+"/releases?per_page=20";
    const long MaxDownload=300L*1024*1024,MaxExpanded=512L*1024*1024;
    readonly string root;
    readonly EventWaitHandle request;
    readonly Thread worker;
    volatile bool stopped,queued;
    string declined;
    internal readonly string EventName="Local\\DWG2-Update-"+Guid.NewGuid().ToString("N");
    internal sealed class Release {internal string Tag,Url,Sha256;internal long Bytes;}
    internal UpdateService(string folder) {
        root=Path.GetFullPath(folder);
        request=new EventWaitHandle(false,EventResetMode.AutoReset,EventName);
        worker=new Thread(Work);worker.IsBackground=true;worker.SetApartmentState(ApartmentState.STA);
    }
    internal void Start() {worker.Start();}
    void Log(string message) {
        try {Directory.CreateDirectory(Path.Combine(root,"logs"));File.AppendAllText(Path.Combine(root,"logs","updates.log"),DateTime.Now.ToString("o")+" "+message+Environment.NewLine);} catch {}
    }
    internal static long VersionNumber(string text) {
        if(text==null)return -1;
        var m=Regex.Match(text,@"^v?(\d+)(?:\.(\d+))?(?:\.(\d+))?(?:-alpha(?:\.\d+)?)?$",RegexOptions.IgnoreCase);
        if(!m.Success)return -1;
        int a,b=0,c=0;if(!int.TryParse(m.Groups[1].Value,out a)||a>999)return -1;
        if(m.Groups[2].Success&&!int.TryParse(m.Groups[2].Value,out b))return -1;
        if(m.Groups[3].Success&&!int.TryParse(m.Groups[3].Value,out c))return -1;
        if(!m.Groups[2].Success){b=a;a=0;} // v03 and v0.3 are equivalent.
        if(b>999||c>999)return -1;return a*1000000L+b*1000L+c;
    }
    static string StringValue(Dictionary<string,object> value,string name) {object v;return value.TryGetValue(name,out v)?v as string:null;}
    internal static Release Select(string json,string current) {
        var serializer=new JavaScriptSerializer{MaxJsonLength=1024*1024,RecursionLimit=32};
        var releases=serializer.DeserializeObject(json) as object[];if(releases==null)throw new InvalidDataException("Invalid release list.");
        Release best=null;long currentNumber=VersionNumber(current),bestNumber=currentNumber;
        foreach(var item in releases) {
            var release=item as Dictionary<string,object>;if(release==null)continue;
            object draft;if(release.TryGetValue("draft",out draft)&&draft is bool&&(bool)draft)continue;
            string tag=StringValue(release,"tag_name");long number=VersionNumber(tag);if(number<=bestNumber)continue;
            object list;if(!release.TryGetValue("assets",out list))continue;
            var assets=list as object[];if(assets==null)continue;Release candidate=null;int matches=0;
            foreach(var assetItem in assets) {
                var asset=assetItem as Dictionary<string,object>;if(asset==null)continue;
                string name=StringValue(asset,"name");
                if(name==null||!name.StartsWith("DWG2Recomp-",StringComparison.Ordinal)||!name.EndsWith("-player.zip",StringComparison.Ordinal))continue;
                ++matches;string hash=StringValue(asset,"digest"),url=StringValue(asset,"browser_download_url");object size;
                long bytes;if(!asset.TryGetValue("size",out size)||!long.TryParse(Convert.ToString(size),out bytes)||bytes<=0||bytes>MaxDownload)continue;
                if(hash==null||!Regex.IsMatch(hash,@"^sha256:[0-9a-fA-F]{64}$")||!AllowedUrl(url))continue;
                candidate=new Release{Tag=tag,Url=url,Sha256=hash.Substring(7).ToLowerInvariant(),Bytes=bytes};
            }
            if(matches==1&&candidate!=null){best=candidate;bestNumber=number;}
        }
        return best;
    }
    internal static bool AllowedUrl(string value) {
        Uri uri;if(!Uri.TryCreate(value,UriKind.Absolute,out uri)||uri.Scheme!="https"||!uri.IsDefaultPort||uri.UserInfo.Length!=0)return false;
        string host=uri.DnsSafeHost;
        return host=="api.github.com"||host=="github.com"||host.EndsWith(".githubusercontent.com",StringComparison.OrdinalIgnoreCase);
    }
    static HttpWebResponse Open(string url,int timeout) {
        for(int redirects=0;redirects<=5;++redirects) {
            if(!AllowedUrl(url))throw new InvalidDataException("Unexpected update download address.");
            var request=(HttpWebRequest)WebRequest.Create(url);request.UserAgent="DWG2Recomp/"+Version;
            request.Accept="application/vnd.github+json";request.AllowAutoRedirect=false;
            request.Timeout=timeout;request.ReadWriteTimeout=timeout;
            var response=(HttpWebResponse)request.GetResponse();int status=(int)response.StatusCode;
            if(status>=300&&status<400){string location=response.Headers["Location"];response.Dispose();url=new Uri(new Uri(url),location).AbsoluteUri;continue;}
            return response;
        }
        throw new IOException("Too many update redirects.");
    }
    static string Metadata() {
        using(var response=Open(Releases,4000))using(var stream=response.GetResponseStream())using(var output=new MemoryStream()) {
            byte[] buffer=new byte[8192];int read;
            while((read=stream.Read(buffer,0,buffer.Length))>0){if(output.Length+read>1024*1024)throw new InvalidDataException("Release response too large.");output.Write(buffer,0,read);}
            return Encoding.UTF8.GetString(output.ToArray());
        }
    }
    void Work() {
        // Automatic check is concurrent with normal game launch. No waiting for it.
        bool manual=false;
        while(!stopped) {
            try {
                if(queued){if(manual)MessageBox.Show("An update is downloaded and will install after you close the game.","DWG2 Updates");}
                else {
                    ServicePointManager.SecurityProtocol|=SecurityProtocolType.Tls12;
                    Release release=Select(Metadata(),Version);
                    if(!stopped) {
                        if(release==null){if(manual)MessageBox.Show("You have the latest available build.","DWG2 Updates");Log("No newer compatible player package.");}
                        else if(manual||declined!=release.Tag) {
                            using(var prompt=new UpdatePrompt(root,release.Tag)) {
                                if(prompt.ShowDialog()==DialogResult.Yes)DownloadAndQueue(release);
                                else {declined=release.Tag;Log("Update declined: "+release.Tag);}
                            }
                        }
                    }
                }
            } catch(Exception error) {
                Log("Update check/download failed: "+error.GetType().Name+": "+error.Message);
                if(manual&&!stopped)MessageBox.Show("Could not check or download the update. You can keep using this build.\n\nCheck your connection and that the repository releases are public.","DWG2 Updates",MessageBoxButtons.OK,MessageBoxIcon.Information);
            }
            if(stopped)return;request.WaitOne();manual=true;
        }
    }
    void DownloadAndQueue(Release release) {
        string work=Path.Combine(root,".updates",Guid.NewGuid().ToString("N"));RequirePlainParents(work);Directory.CreateDirectory(work);
        string zip=Path.Combine(work,"player.zip"),stage=Path.Combine(work,"package");
        using(var response=Open(release.Url,15000))using(var input=response.GetResponseStream())using(var output=new FileStream(zip,FileMode.CreateNew,FileAccess.Write,FileShare.None)) {
            if(response.ContentLength>=0&&response.ContentLength!=release.Bytes)throw new InvalidDataException("Update length mismatch.");
            byte[] buffer=new byte[65536];long total=0;int read;
            while((read=input.Read(buffer,0,buffer.Length))>0){if(stopped)throw new OperationCanceledException();total+=read;if(total>release.Bytes)throw new InvalidDataException("Update exceeds declared size.");output.Write(buffer,0,read);}
            if(total!=release.Bytes)throw new InvalidDataException("Incomplete update download.");output.Flush(true);
        }
        VerifyDownloadedFile(zip,release);
        Extract(zip,stage);
        File.Delete(zip); // The verified extracted package is sufficient for installation.
        string helper=Path.Combine(stage,"Apply-DownloadedUpdate.ps1");
        if(!File.Exists(helper)||!File.Exists(Path.Combine(stage,"apply-update.ps1"))||!File.Exists(Path.Combine(stage,"update.json"))||!File.Exists(Path.Combine(stage,"START_DWG2.exe")))throw new InvalidDataException("Incomplete player update.");
        // Process IDs only; no shell text from release metadata is executed.
        string command="& '"+helper.Replace("'","''")+"' -GameFolder '"+root.Replace("'","''")+"' -Package '"+stage.Replace("'","''")+"' -LauncherProcessId "+Process.GetCurrentProcess().Id;
        var info=new ProcessStartInfo(Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.System),"WindowsPowerShell","v1.0","powershell.exe"),"-NoProfile -ExecutionPolicy Bypass -EncodedCommand "+Convert.ToBase64String(Encoding.Unicode.GetBytes(command))){UseShellExecute=false,CreateNoWindow=true,WindowStyle=ProcessWindowStyle.Hidden};
        Process.Start(info);queued=true;Log("Verified update queued: "+release.Tag);
        MessageBox.Show("Update downloaded and verified. It will install when you close the game and launcher.\n\nYou can continue playing now; save your progress before quitting.","DWG2 Updates");
    }
    internal static void Extract(string zip,string folder) {
        RequirePlainParents(folder);Directory.CreateDirectory(folder);string prefix=Path.GetFullPath(folder)+Path.DirectorySeparatorChar;
        var seen=new HashSet<string>(StringComparer.OrdinalIgnoreCase);long total=0;
        using(var archive=ZipFile.OpenRead(zip)) {
            if(archive.Entries.Count>10000)throw new InvalidDataException("Too many update files.");
            foreach(var entry in archive.Entries) {
                string name=entry.FullName.Replace('/',Path.DirectorySeparatorChar);
                if(name.IndexOf(':')>=0||Path.IsPathRooted(name)||((entry.ExternalAttributes>>16)&0xF000)==0xA000)throw new InvalidDataException("Unsafe update entry.");
                string target=Path.GetFullPath(Path.Combine(folder,name));
                if(!target.StartsWith(prefix,StringComparison.OrdinalIgnoreCase)||!seen.Add(target))throw new InvalidDataException("Escaping or duplicate update entry.");
                total+=entry.Length;if(entry.Length<0||total>MaxExpanded)throw new InvalidDataException("Expanded update too large.");
                if(entry.Name.Length==0){Directory.CreateDirectory(target);continue;}
                Directory.CreateDirectory(Path.GetDirectoryName(target));using(var input=entry.Open())using(var output=new FileStream(target,FileMode.CreateNew,FileAccess.Write))input.CopyTo(output);
            }
        }
    }
    static void RequirePlainParents(string path) {
        var directory=new DirectoryInfo(Path.GetFullPath(path));
        while(directory!=null){if(directory.Exists&&(directory.Attributes&FileAttributes.ReparsePoint)!=0)throw new InvalidDataException("Linked update folders are not supported.");directory=directory.Parent;}
    }
    internal static void VerifyDownloadedFile(string path,Release release) {
        using(var file=File.OpenRead(path))using(var sha=SHA256.Create()) {
            if(file.Length!=release.Bytes)throw new InvalidDataException("Update length mismatch.");
            string actual=BitConverter.ToString(sha.ComputeHash(file)).Replace("-","").ToLowerInvariant();
            if(actual!=release.Sha256)throw new InvalidDataException("Update SHA256 mismatch.");
        }
    }
    public void Dispose() {stopped=true;request.Set();/* Worker owns any active HTTP call; process exit cancels it. */}
}

internal sealed class UpdatePrompt : Form {
    [DllImport("user32.dll",CharSet=CharSet.Unicode)]static extern bool SetProp(IntPtr window,string name,IntPtr value);
    [DllImport("user32.dll",CharSet=CharSet.Unicode)]static extern IntPtr RemoveProp(IntPtr window,string name);
    protected override void WndProc(ref Message message) {
        if(message.Msg==0x8319 && Form.ActiveForm==this) {
            if(message.WParam.ToInt32()==1){DialogResult=DialogResult.Yes;Close();return;}
            if(message.WParam.ToInt32()==2){DialogResult=DialogResult.No;Close();return;}
        }
        base.WndProc(ref message);
    }
    internal UpdatePrompt(string root,string tag) {
        Text="DWG2 — Update available";ClientSize=new Size(500,220);FormBorderStyle=FormBorderStyle.FixedDialog;MaximizeBox=false;MinimizeBox=false;StartPosition=FormStartPosition.CenterScreen;BackColor=Color.FromArgb(16,25,43);ForeColor=Color.White;Font=new Font("Segoe UI",10);
        Controls.Add(new Label{Text="DWG2 Recomp "+tag+" is available",Location=new Point(24,22),Size=new Size(455,30),Font=new Font("Segoe UI",13,FontStyle.Bold)});
        Controls.Add(new Label{Text="Download now and install after you close the game.\nDecline to keep playing the current build.",Location=new Point(24,68),Size=new Size(455,54)});
        var yes=new Button{Text="✕ / A  —  Update",DialogResult=DialogResult.Yes,Location=new Point(24,148),Size=new Size(215,46),BackColor=Color.FromArgb(200,160,55),ForeColor=Color.Black};
        var no=new Button{Text="○ / B  —  Not now",DialogResult=DialogResult.No,Location=new Point(260,148),Size=new Size(215,46)};
        Controls.Add(yes);Controls.Add(no);AcceptButton=yes;CancelButton=no;ActiveControl=no;
        Shown+=delegate {SetProp(Handle,"DWG2.UpdatePrompt",new IntPtr(1));};
        FormClosed+=delegate {RemoveProp(Handle,"DWG2.UpdatePrompt");};
    }
}
