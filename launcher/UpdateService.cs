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
using System.Threading.Tasks;
using System.Web.Script.Serialization;
using System.Windows.Forms;

// Original launcher updater. Network/disk work never runs on the game thread.
internal sealed class UpdateService : IDisposable {
    internal const string Version="0.7-alpha";
    internal const string EventVariable="DWG2_UPDATE_EVENT";
    internal const string Repository="Adeelshah001/DWG2Recomp";
    internal const string Releases="https://api.github.com/repos/"+Repository+"/releases?per_page=20";
    const long MaxDownload=300L*1024*1024,MaxExpanded=512L*1024*1024;
    readonly string root;
    internal const string ModalVariable="DWG2_UPDATE_MODAL_EVENT";
    internal readonly string ModalName="Local\\DWG2-UpdateModal-"+Guid.NewGuid().ToString("N");
    readonly EventWaitHandle modal;
    readonly bool testChannel=Environment.GetEnvironmentVariable("DWG2_TEST_RELEASES")=="1";
    readonly EventWaitHandle request;
    readonly Thread worker;
    volatile bool stopped,queued;
    string declined;
    readonly object gameGate=new object();
    Process game;
    internal bool InstallationQueued { get { return queued; } }
    internal void SetGame(Process value) {lock(gameGate)game=value;}
    [DllImport("user32.dll",SetLastError=true)]static extern bool PostMessage(IntPtr window,uint message,IntPtr w,IntPtr l);
    internal static bool RequestNormalClose(Process process) {
        if(process==null)return true;
        try {if(process.HasExited)return true;process.Refresh();IntPtr window=process.MainWindowHandle;
            return PostNormalClose(window);
        } catch(InvalidOperationException){return true;}
    }
    internal static bool PostNormalClose(IntPtr window) {return window!=IntPtr.Zero&&PostMessage(window,0x0010,IntPtr.Zero,IntPtr.Zero);}
    internal static string InstallPromptText="The update is ready. Close the game and launcher to install it now?\n\nSave your progress before choosing Yes. No keeps playing and installs after you quit. The updated launcher will reopen automatically.";
    internal bool CloseForInstall() {lock(gameGate)return RequestNormalClose(game);}

    internal readonly string EventName="Local\\DWG2-Update-"+Guid.NewGuid().ToString("N");
    internal sealed class Release {internal string Tag,Url,Sha256;internal long Bytes;internal bool Compact;internal Release FullPackage;}
    internal UpdateService(string folder) {
        root=Path.GetFullPath(folder);
        modal=new EventWaitHandle(false,EventResetMode.ManualReset,ModalName);
        request=new EventWaitHandle(false,EventResetMode.AutoReset,EventName);
        worker=new Thread(Work);worker.IsBackground=true;worker.SetApartmentState(ApartmentState.STA);
    }
    internal void Start() {worker.Start();}
    void Log(string message) {
        try {Directory.CreateDirectory(Path.Combine(root,"logs"));File.AppendAllText(Path.Combine(root,"logs","updates.log"),DateTime.Now.ToString("o")+" "+message+Environment.NewLine);} catch {}
    }
    internal static long VersionNumber(string text) {
        if(text==null)return -1;
        var m=Regex.Match(text,@"^v?(\d+)(?:\.(\d+))?(?:\.(\d+))?(?:-(?:alpha|test)(?:\.\d+)?)?$",RegexOptions.IgnoreCase);
        if(!m.Success)return -1;
        int a,b=0,c=0;if(!int.TryParse(m.Groups[1].Value,out a)||a>999)return -1;
        if(m.Groups[2].Success&&!int.TryParse(m.Groups[2].Value,out b))return -1;
        if(m.Groups[3].Success&&!int.TryParse(m.Groups[3].Value,out c))return -1;
        if(!m.Groups[2].Success){b=a;a=0;} // v03 and v0.3 are equivalent.
        if(b>999||c>999)return -1;return a*1000000L+b*1000L+c;
    }
    static string StringValue(Dictionary<string,object> value,string name) {object v;return value.TryGetValue(name,out v)?v as string:null;}
    internal static Release Select(string json,string current) {return Select(json,current,false);}
    internal static Release Select(string json,string current,bool tests) {
        var serializer=new JavaScriptSerializer{MaxJsonLength=1024*1024,RecursionLimit=32};
        var releases=serializer.DeserializeObject(json) as object[];if(releases==null)throw new InvalidDataException("Invalid release list.");
        Release best=null;long currentNumber=VersionNumber(current),bestNumber=currentNumber;
        foreach(var item in releases) {
            var release=item as Dictionary<string,object>;if(release==null)continue;
            object draft;if(release.TryGetValue("draft",out draft)&&draft is bool&&(bool)draft)continue;
            string tag=StringValue(release,"tag_name");
            object pre;if(!tests&&((tag!=null&&tag.Contains("-test"))||(release.TryGetValue("prerelease",out pre)&&pre is bool&&(bool)pre)))continue;
            long number=VersionNumber(tag);if(number<=bestNumber)continue;
            object list;if(!release.TryGetValue("assets",out list))continue;
            var assets=list as object[];if(assets==null)continue;Release candidate=null,compact=null;int matches=0,compactMatches=0;
            foreach(var assetItem in assets) {
                var asset=assetItem as Dictionary<string,object>;if(asset==null)continue;
                string name=StringValue(asset,"name");
                if(name==null||(name!="DWG2Recomp.zip"&&!name.StartsWith("DWG2Recomp-",StringComparison.Ordinal)))continue;
                bool small=name.EndsWith("-update.zip",StringComparison.Ordinal);
                if(!small&&name!="DWG2Recomp.zip"&&!name.EndsWith("-player.zip",StringComparison.Ordinal))continue;
                if(small)++compactMatches;else ++matches;
                string hash=StringValue(asset,"digest"),url=StringValue(asset,"browser_download_url");object size;
                long bytes;if(!asset.TryGetValue("size",out size)||!long.TryParse(Convert.ToString(size),out bytes)||bytes<=0||bytes>MaxDownload)continue;
                if(hash==null||!Regex.IsMatch(hash,@"^sha256:[0-9a-fA-F]{64}$")||!AllowedUrl(url))continue;
                var parsed=new Release{Tag=tag,Url=url,Sha256=hash.Substring(7).ToLowerInvariant(),Bytes=bytes,Compact=small};
                if(small)compact=parsed;else candidate=parsed;
            }
            if(matches==1&&candidate!=null){if(compactMatches==1&&compact!=null&&compact.Bytes<candidate.Bytes){compact.FullPackage=candidate;candidate=compact;}best=candidate;bestNumber=number;}
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
            bool accepted=false;
            try {
                if(queued){if(manual)ShowInformation("Update ready","The verified update will install after you close the game and launcher normally.");}
                else {
                    ServicePointManager.SecurityProtocol|=SecurityProtocolType.Tls12;
                    ShowCompletedUpdate();
                    string metadata=null;
                    if(manual)RunProgress("Checking for updates",delegate(Action<string,long,long> report) {report("Checking GitHub for a newer release...",0,0);metadata=Metadata();});
                    else metadata=Metadata();
                    Release release=Select(metadata,Version,testChannel);
                    if(!stopped) {
                        if(release==null){if(manual)ShowInformation("No update available","You have the latest available build.");Log("No newer compatible player package.");}
                        else if(manual||declined!=release.Tag) {
                            using(var prompt=new UpdatePrompt(root,release.Tag)) {
                                if(ShowPrompt(prompt)==DialogResult.Yes){accepted=true;DownloadAndQueue(release);}
                                else {declined=release.Tag;Log("Update declined: "+release.Tag);}
                            }
                        }
                    }
                }
            } catch(Exception error) {
                Log("Update check/download failed: "+error.GetType().Name+": "+error.Message);
                if((manual||accepted)&&!stopped)ShowInformation("Update could not finish","Your current installation is retained. You can keep playing.\n\nCheck your connection and see logs/updates.log for details.");
            }
            if(stopped)return;request.WaitOne();manual=true;
        }
    }
    [DllImport("user32.dll")]static extern bool EnableWindow(IntPtr window,bool enabled);
    [DllImport("user32.dll")]static extern bool IsWindowEnabled(IntPtr window);
    IntPtr GameWindow() {
        lock(gameGate)try {if(game==null||game.HasExited)return IntPtr.Zero;game.Refresh();return game.MainWindowHandle;}
        catch(InvalidOperationException){return IntPtr.Zero;}
    }
    [DllImport("user32.dll",EntryPoint="SetWindowLongPtrW")]static extern IntPtr SetWindowOwner(IntPtr window,int index,IntPtr owner);
    internal DialogResult ShowPrompt(UpdatePrompt prompt) {return ShowOwned(prompt);}
    internal DialogResult ShowOwned(Form prompt) {
        modal.Set();var disabled=new HashSet<IntPtr>();IntPtr owner=IntPtr.Zero;
        Action attach=delegate {
            IntPtr window=GameWindow();
            if(window!=IntPtr.Zero) {
                if(IsWindowEnabled(window)){EnableWindow(window,false);disabled.Add(window);}
                if(window!=owner && prompt.IsHandleCreated) {owner=window;SetWindowOwner(prompt.Handle,-8,window);prompt.BringToFront();prompt.Activate();}
            }
        };
        var timer=new System.Windows.Forms.Timer{Interval=150};timer.Tick+=delegate {attach();};
        EventHandler shown=delegate {attach();prompt.BringToFront();prompt.Activate();timer.Start();};
        prompt.TopMost=true;prompt.Shown+=shown;
        Log("Update popup input capture enabled.");
        try {return prompt.ShowDialog();}
        finally {timer.Stop();timer.Dispose();prompt.Shown-=shown;foreach(IntPtr window in disabled)EnableWindow(window,true);modal.Reset();Log("Update popup input capture released; native input waits for neutral.");}
    }
    void ShowInformation(string title,string message) {
        using(var prompt=UpdatePrompt.Information(title,message))ShowPrompt(prompt);
    }
    void RunProgress(string title,Action<Action<string,long,long>> operation) {
        using(var progress=new UpdateTransferProgress(title,operation)) {ShowOwned(progress);if(progress.Failure!=null)throw progress.Failure;}
    }
    internal void ShowCompletedUpdate() {
        string receipt=Path.Combine(root,"logs","update-completed.json");
        if(!File.Exists(receipt))return;
        var info=new FileInfo(receipt);if(info.Length>4096)throw new InvalidDataException("Invalid update completion record.");
        string original=File.ReadAllText(receipt);
        var state=new JavaScriptSerializer().DeserializeObject(original) as Dictionary<string,object>;
        string version=state==null?null:StringValue(state,"version");
        if(version==null||VersionNumber(version)<0)throw new InvalidDataException("Invalid update completion version.");
        // Defer the "Launcher updated" notification until after preparation and game launch.
        // The update progress window (Apply-DownloadedUpdate.ps1) already shows continuous
        // progress until the game window is visible. Showing another prompt here would be
        // redundant and appear before preparation completes.
        // Just log and delete the receipt; the player already saw the update progress.
        Log("Update completed: "+version);
        if(File.Exists(receipt)&&File.ReadAllText(receipt)==original)File.Delete(receipt);
    }
    void DownloadAndQueue(Release release) {
        string work=Path.Combine(root,".updates",Guid.NewGuid().ToString("N"));RequirePlainParents(work);Directory.CreateDirectory(work);
        string zip=Path.Combine(work,"player.zip"),stage=Path.Combine(work,"package");
        RunProgress("Downloading update",delegate(Action<string,long,long> report) {
        using(var response=Open(release.Url,15000))using(var input=response.GetResponseStream())using(var output=new FileStream(zip,FileMode.CreateNew,FileAccess.Write,FileShare.None)) {
            if(response.ContentLength>=0&&response.ContentLength!=release.Bytes)throw new InvalidDataException("Update length mismatch.");
            byte[] buffer=new byte[65536];long total=0;int read;
            report("Downloading verified player update...",0,release.Bytes);
            while((read=input.Read(buffer,0,buffer.Length))>0){if(stopped)throw new OperationCanceledException();total+=read;if(total>release.Bytes)throw new InvalidDataException("Update exceeds declared size.");output.Write(buffer,0,read);report("Downloading player update...",total,release.Bytes);}
            if(total!=release.Bytes)throw new InvalidDataException("Incomplete update download.");output.Flush(true);
        }
        report("Checking the update download...",0,0);VerifyDownloadedFile(zip,release);
        report("Extracting update files...",0,0);Extract(zip,stage);
        });
        File.Delete(zip); // The verified extracted package is sufficient for installation.
        stage=PlayerPackageRoot(stage);
        if(!HydratePreparation(root,stage)) {
            if(!release.Compact||release.FullPackage==null)throw new InvalidDataException("Installed preparation files differ from the update requirements.");
            Log("Compact update needs different preparation tools; downloading matching full package.");
            DownloadAndQueue(release.FullPackage);return;
        }
        string helper=PlayerUpdateHelper(stage);
        // Process IDs only; no shell text from release metadata is executed.
        string command="& '"+helper.Replace("'","''")+"' -GameFolder '"+root.Replace("'","''")+"' -Package '"+stage.Replace("'","''")+"' -LauncherProcessId "+Process.GetCurrentProcess().Id+" -ClosePromptHandled";
        var info=new ProcessStartInfo(Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.System),"WindowsPowerShell","v1.0","powershell.exe"),"-NoProfile -ExecutionPolicy Bypass -EncodedCommand "+Convert.ToBase64String(Encoding.Unicode.GetBytes(command))){UseShellExecute=false,CreateNoWindow=true,WindowStyle=ProcessWindowStyle.Hidden};
        using(var installer=Process.Start(info)) {
            if(installer==null)throw new IOException("The update installer could not be started.");
        }
        queued=true;Log("Verified update queued: "+release.Tag);
        using(var installPrompt=new UpdatePrompt(root,release.Tag,true))
        if(!stopped&&ShowPrompt(installPrompt)==DialogResult.Yes) {
            if(!CloseForInstall())ShowInformation("Close the game to update","Please save and close the game normally. The verified update will install after the launcher exits.");
            else Log("Normal game close requested for installation.");
        }
    }
    internal static bool HydratePreparation(string installed,string stage) {
        string file=Path.Combine(stage,"maintenance","update.json");if(!File.Exists(file))file=Path.Combine(stage,"update.json");
        if(!File.Exists(file))throw new InvalidDataException("Missing update manifest.");
        if(new FileInfo(file).Length>1024*1024)throw new InvalidDataException("Update manifest too large.");
        var manifest=new JavaScriptSerializer{MaxJsonLength=1024*1024}.DeserializeObject(File.ReadAllText(file)) as Dictionary<string,object>;
        if(manifest==null)throw new InvalidDataException("Invalid update manifest.");
        object value;if(!manifest.TryGetValue("reusePreparation",out value))return true;
        var entries=value as object[];if(entries==null||entries.Length!=3)throw new InvalidDataException("Invalid preparation reuse list.");
        object declared;var files=manifest.TryGetValue("files",out declared)?declared as object[]:null;
        if(files==null)throw new InvalidDataException("Missing managed update files.");
        var hashes=new Dictionary<string,string>(StringComparer.Ordinal);
        foreach(var item in files){var f=item as Dictionary<string,object>;if(f==null)throw new InvalidDataException("Invalid managed file.");string name=StringValue(f,"path"),hash=StringValue(f,"sha256");if(name==null||hash==null||hashes.ContainsKey(name))throw new InvalidDataException("Duplicate managed file.");hashes.Add(name,hash);}
        var seen=new HashSet<string>(StringComparer.Ordinal);
        foreach(var item in entries) {
            var f=item as Dictionary<string,object>;if(f==null)throw new InvalidDataException("Invalid preparation reuse entry.");
            string name=StringValue(f,"path"),hash=StringValue(f,"sha256"),expected;
            if((name!="preparation/build.json"&&name!="preparation/package.json"&&name!="preparation/tools.zip")||!seen.Add(name)||hash==null||!Regex.IsMatch(hash,"^[0-9a-f]{64}$")||!hashes.TryGetValue(name,out expected)||hash!=expected)throw new InvalidDataException("Unsafe or inconsistent preparation reuse entry.");
            string source=Path.Combine(installed,name.Replace('/',Path.DirectorySeparatorChar));RequirePlainParents(source);
            if(!File.Exists(source)||FileHash(source)!=hash)return false;
        }
        foreach(var item in entries) {
            var f=(Dictionary<string,object>)item;string name=StringValue(f,"path"),hash=StringValue(f,"sha256");
            string source=Path.Combine(installed,name.Replace('/',Path.DirectorySeparatorChar)),destination=Path.Combine(stage,name.Replace('/',Path.DirectorySeparatorChar));RequirePlainParents(destination);
            if(File.Exists(destination)){if(FileHash(destination)!=hash)throw new InvalidDataException("Conflicting staged preparation file.");continue;}
            Directory.CreateDirectory(Path.GetDirectoryName(destination));File.Copy(source,destination);
            if(FileHash(destination)!=hash)throw new InvalidDataException("Preparation file changed during update staging.");
        }
        return true;
    }
    static string FileHash(string path) {using(var f=File.OpenRead(path))using(var sha=SHA256.Create())return BitConverter.ToString(sha.ComputeHash(f)).Replace("-","").ToLowerInvariant();}
    internal static string PlayerPackageRoot(string stage) {
        RequirePlainParents(stage);
        if(File.Exists(Path.Combine(stage,"SETUP_DWG2.exe"))||File.Exists(Path.Combine(stage,"DWG2R.exe"))||Directory.Exists(Path.Combine(stage,"maintenance")))return stage;
        string found=null;
        foreach(string child in Directory.GetDirectories(stage))if((File.Exists(Path.Combine(child,"SETUP_DWG2.exe"))||File.Exists(Path.Combine(child,"DWG2R.exe")))&&File.Exists(Path.Combine(child,"maintenance","update.json"))) {
            RequirePlainParents(child);if(found!=null)throw new InvalidDataException("Ambiguous player folders.");found=child;
        }
        return found??stage;
    }
    internal static string PlayerUpdateHelper(string stage) {
        stage=PlayerPackageRoot(stage);
        string support=Directory.Exists(Path.Combine(stage,"maintenance"))?Path.Combine(stage,"maintenance"):stage;
        string helper=Path.Combine(support,"Apply-DownloadedUpdate.ps1");
        if(!File.Exists(helper)||!File.Exists(Path.Combine(support,"apply-update.ps1"))||!File.Exists(Path.Combine(support,"update.json"))||
           (!File.Exists(Path.Combine(stage,"SETUP_DWG2.exe"))&&!File.Exists(Path.Combine(stage,"DWG2R.exe"))&&!File.Exists(Path.Combine(stage,"START_DWG2.exe"))))throw new InvalidDataException("Incomplete player update.");
        return helper;
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
    public void Dispose() {stopped=true;modal.Reset();request.Set();/* Worker owns any active HTTP call; process exit cancels it. */}
}

internal sealed class UpdatePrompt : Form {
    bool information;
    [DllImport("user32.dll",CharSet=CharSet.Unicode)]static extern bool SetProp(IntPtr window,string name,IntPtr value);
    [DllImport("user32.dll",CharSet=CharSet.Unicode)]static extern IntPtr RemoveProp(IntPtr window,string name);
    protected override void WndProc(ref Message message) {
        if(message.Msg==0x8319 && Form.ActiveForm==this) {
            if(message.WParam.ToInt32()==1){DialogResult=information?DialogResult.OK:DialogResult.Yes;Close();return;}
            if(message.WParam.ToInt32()==2){DialogResult=information?DialogResult.OK:DialogResult.No;Close();return;}
        }
        base.WndProc(ref message);
    }
    internal UpdatePrompt(string root,string tag):this(root,tag,false) {}
    internal UpdatePrompt(string root,string tag,bool install) {
        Text=install?"DWG2 — Install update":"DWG2 — Update available";ClientSize=new Size(500,220);FormBorderStyle=FormBorderStyle.FixedDialog;MaximizeBox=false;MinimizeBox=false;StartPosition=FormStartPosition.CenterScreen;BackColor=Color.FromArgb(16,25,43);ForeColor=Color.White;Font=new Font("Segoe UI",10);
        Controls.Add(new Label{Text=install?"Close and install the update?":"DWG2 Recomp "+tag+" is available",Location=new Point(24,22),Size=new Size(455,30),Font=new Font("Segoe UI",13,FontStyle.Bold)});
        Controls.Add(new Label{Text=install?"Save your progress first. Yes closes the game and launcher, installs the update, then reopens it. No installs after you quit.":"Download the update now. You can choose when to close and install after it is verified.",Location=new Point(24,68),Size=new Size(455,54)});
        var yes=new Button{Text=install?"✕ / A  —  Close and install":"✕ / A  —  Update",DialogResult=DialogResult.Yes,Location=new Point(24,148),Size=new Size(215,46),BackColor=Color.FromArgb(200,160,55),ForeColor=Color.Black};
        var no=new Button{Text=install?"○ / B  —  Later":"○ / B  —  Not now",DialogResult=DialogResult.No,Location=new Point(260,148),Size=new Size(215,46)};
        Controls.Add(yes);Controls.Add(no);AcceptButton=yes;CancelButton=no;ActiveControl=no;
        Shown+=delegate {SetProp(Handle,"DWG2.UpdatePrompt",new IntPtr(1));};
        FormClosed+=delegate {RemoveProp(Handle,"DWG2.UpdatePrompt");};
    }
    internal static UpdatePrompt Information(string title,string message) {
        var form=new UpdatePrompt("","",false);form.information=true;form.Text="DWG2 — "+title;
        form.Controls[0].Text=title;form.Controls[1].Text=message;form.Controls[1].Height=76;
        var yes=(Button)form.AcceptButton;var no=(Button)form.CancelButton;
        no.Visible=false;yes.Text="✕ / A  —  Continue";yes.DialogResult=DialogResult.OK;yes.Width=451;yes.Top=162;
        form.ClientSize=new Size(500,230);form.AcceptButton=yes;form.CancelButton=yes;form.ActiveControl=yes;return form;
    }
}

// Owns a message loop while network/disk work runs on a background task. The
// enclosing UpdateService holds the native input gate until the form closes.
internal sealed class UpdateTransferProgress : Form {
    readonly Label status;readonly SetupBar bar;readonly Action<Action<string,long,long>> operation;
    internal Exception Failure;bool done;int lastPercent=-2;string lastStage="";
    internal UpdateTransferProgress(string title,Action<Action<string,long,long>> work) {
        operation=work;Text="DWG2 — "+title;ClientSize=new Size(540,165);StartPosition=FormStartPosition.CenterScreen;
        FormBorderStyle=FormBorderStyle.FixedDialog;MaximizeBox=false;MinimizeBox=false;ControlBox=false;
        BackColor=Color.FromArgb(13,27,44);ForeColor=Color.FromArgb(241,245,250);Font=new Font("Segoe UI",10);
        Controls.Add(new Label{Text=title,Location=new Point(24,20),Size=new Size(492,30),Font=new Font("Segoe UI",13,FontStyle.Bold)});
        status=new Label{Text="Starting...",Location=new Point(24,60),Size=new Size(492,30)};
        bar=new SetupBar{Location=new Point(24,105),Size=new Size(492,24),Marquee=true};Controls.Add(status);Controls.Add(bar);
        FormClosing+=delegate(object s,FormClosingEventArgs e){if(!done)e.Cancel=true;};
        Shown+=async delegate {
            try {await Task.Run(delegate {operation(Report);});}catch(Exception e){Failure=e;}
            done=true;Close();
        };
    }
    internal static int Percentage(long completed,long total) {
        if(total<=0)return -1;if(completed<0||completed>total)throw new InvalidDataException("Invalid update progress.");
        return (int)(100m*completed/total);
    }
    void Report(string stage,long completed,long total) {
        int percent=Percentage(completed,total);
        if(percent==lastPercent&&stage==lastStage)return;lastPercent=percent;lastStage=stage;
        if(!IsDisposed&&IsHandleCreated)BeginInvoke((Action)delegate {
            if(IsDisposed)return;status.Text=stage+(percent<0?"":"  "+percent+"%");
            bar.Marquee=percent<0;if(percent>=0)bar.Value=percent;
        });
    }
}
