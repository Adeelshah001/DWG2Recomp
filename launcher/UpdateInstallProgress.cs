using System;using System.IO;using System.Diagnostics;using System.Drawing;using System.Runtime.InteropServices;using System.Web.Script.Serialization;using System.Windows.Forms;

// Standalone update display: survives launcher replacement and uses the SAME
// SetupBar artwork as first-time setup. It only reads bounded status records.
internal sealed class UpdateInstallProgress : Form {
    readonly Label caption,status;readonly SetupBar bar;readonly Timer poll=new Timer{Interval=100};
    readonly string path;readonly int worker;readonly bool preview;bool terminal;DateTime finished;
    [DllImport("user32.dll")]static extern bool ShowWindow(IntPtr window,int command);
    [DllImport("user32.dll")]static extern bool SetForegroundWindow(IntPtr window);
    internal void ShowVisible() {Show();WindowState=FormWindowState.Normal;ShowWindow(Handle,5);BringToFront();Activate();SetForegroundWindow(Handle);}
    internal UpdateInstallProgress(string statePath,int workerPid,bool isPreview=false) {
        path=Path.GetFullPath(statePath);worker=workerPid;preview=isPreview;
        Text="Dynasty Warriors: Gundam 2 — Updating";ClientSize=new Size(620,220);TopMost=true;
        AutoScaleDimensions=new SizeF(96,96);AutoScaleMode=AutoScaleMode.Dpi;
        StartPosition=FormStartPosition.CenterScreen;FormBorderStyle=FormBorderStyle.FixedDialog;
        MaximizeBox=false;MinimizeBox=false;ControlBox=preview;
        BackColor=Color.FromArgb(13,27,44);ForeColor=Color.FromArgb(241,245,250);Font=new Font("Segoe UI",10);
        Controls.Add(new Label{Text="DYNASTY WARRIORS: GUNDAM 2",Location=new Point(30,24),Size=new Size(560,26),Font=new Font("Segoe UI",11,FontStyle.Bold),ForeColor=Color.FromArgb(230,184,92)});
        caption=new Label{Text="Updating...",Location=new Point(30,78),Size=new Size(560,32),Font=new Font("Segoe UI",16)};
        bar=new SetupBar{Location=new Point(30,120),Size=new Size(560,30),Marquee=true};bar.AccessibleName="Update installation progress";
        status=new Label{Text="Preparing the update...",Location=new Point(30,163),Size=new Size(560,36),ForeColor=Color.FromArgb(179,196,213),AutoEllipsis=true};
        Controls.AddRange(new Control[]{caption,bar,status});
        FormClosing+=delegate(object s,FormClosingEventArgs e){if(!terminal&&!preview)e.Cancel=true;};
        poll.Tick+=delegate{ReadStatus();};Shown+=delegate{poll.Start();Activate();};
    }
    void Failed(string message) {terminal=true;bar.Marquee=false;caption.Text="Update needs attention";status.Text=message;ControlBox=true;poll.Stop();}
    void ReadStatus() {
        try {
            if(File.Exists(path)) {
                if(new FileInfo(path).Length>8192){Failed("Invalid update status. See logs/update-install.log.");return;}
                var record=new JavaScriptSerializer().Deserialize<Status>(File.ReadAllText(path));
                if(record==null)return;
                status.Text=(record.message??"").Length>240?record.message.Substring(0,240):record.message;
                bar.Marquee=record.total<=0;
                if(record.total>0)bar.Value=(int)Math.Min(100,Math.Max(0,100m*record.completed/record.total));
                if(record.state=="failed"){Failed(status.Text);return;}
                if(record.state=="done"&&!terminal){terminal=true;finished=DateTime.UtcNow;caption.Text="Update complete";bar.Marquee=false;bar.Value=100;}
                if(terminal&&preview){caption.Text="Preview complete";status.Text="No files installed. Close this window when you have finished checking.";return;}
                if(terminal&&DateTime.UtcNow-finished>TimeSpan.FromMilliseconds(750)){Close();return;}
            }
            if(!terminal)try{using(var p=Process.GetProcessById(worker)){if(p.HasExited)Failed("The update stopped. See logs/update-install.log.");}}catch(ArgumentException){Failed("The update stopped. See logs/update-install.log.");}
        }catch(IOException){}catch(UnauthorizedAccessException){}catch(ArgumentException){}
    }
    protected override void Dispose(bool disposing){if(disposing)poll.Dispose();base.Dispose(disposing);}
    internal sealed class Status {public string state{get;set;}public string message{get;set;}public long completed{get;set;}public long total{get;set;}}
    [STAThread]static int Main(string[] args){if(args.Length<2||args.Length>3)return 2;int pid;if(!int.TryParse(args[1],out pid)||pid<=0)return 2;bool preview=args.Length==3&&args[2]=="--preview";Application.EnableVisualStyles();using(var form=new UpdateInstallProgress(args[0],pid,preview)){form.ShowVisible();Application.Run(form);}return 0;}
}
