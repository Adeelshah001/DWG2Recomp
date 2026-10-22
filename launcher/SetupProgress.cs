using System;
using System.Drawing;
using System.Drawing.Drawing2D;
using System.Threading;
using System.Threading.Tasks;
using System.Windows.Forms;

internal sealed class SetupUpdate {
    internal readonly int Percent;
    internal readonly string Message;
    internal SetupUpdate(int percent,string message) { Percent=percent;Message=message; }
}

// Percentages measure finished work within fixed stages, not guessed elapsed time.
// All ranges share one monotonic counter, including the parallel preparation workers.
internal sealed class SetupProgress {
    sealed class State {
        internal readonly object Gate=new object();
        internal readonly Action<SetupUpdate> Sink;
        internal int Percent;
        internal string Message="";
        internal bool CanCancel=true,Completed;
        internal State(Action<SetupUpdate> sink) { Sink=sink; }
    }
    readonly State state;readonly double first,last;
    internal SetupProgress(Action<SetupUpdate> sink) : this(new State(sink),0,100) {}
    SetupProgress(State shared,double start,double end) { state=shared;first=start;last=end; }
    internal SetupProgress Range(double start,double end) {
        if(start<0 || end>1 || end<start) throw new ArgumentOutOfRangeException("range");
        return new SetupProgress(state,first+(last-first)*start,first+(last-first)*end);
    }
    internal bool CanCancel { get { lock(state.Gate) return state.CanCancel; } }
    internal void DisableCancellation() { lock(state.Gate) state.CanCancel=false; }
    internal void Report(double fraction,string message) {
        int percent=Math.Min(99,(int)Math.Floor(first+(last-first)*Math.Max(0,Math.Min(1,fraction))));
        lock(state.Gate) {
            if(state.Completed || percent<state.Percent || (percent==state.Percent && message==state.Message)) return;
            state.Percent=percent;state.Message=message;state.Sink(new SetupUpdate(percent,message));
        }
    }
    internal void ReportText(string message) {
        lock(state.Gate) {
            if(state.Completed || message==state.Message) return;
            state.Message=message;state.Sink(new SetupUpdate(state.Percent,message));
        }
    }
    internal void Complete() {
        lock(state.Gate) {
            if(state.Completed) return;
            state.Completed=true;state.Percent=100;state.Message="Your game is ready.";
            state.Sink(new SetupUpdate(100,state.Message));
        }
    }
}

internal sealed class SetupBar : Control {
    int value;
    internal int Value { get { return value; } set { this.value=Math.Max(0,Math.Min(100,value));AccessibleDescription=this.value+" percent";Invalidate(); } }
    internal SetupBar() {
        SetStyle(ControlStyles.UserPaint|ControlStyles.AllPaintingInWmPaint|ControlStyles.OptimizedDoubleBuffer,true);
        AccessibleRole=AccessibleRole.ProgressBar;AccessibleName="Installation progress";
    }
    protected override void OnPaint(PaintEventArgs e) {
        var box=new Rectangle(0,0,Width-1,Height-1);
        using(var track=new SolidBrush(Color.FromArgb(22,42,67))) e.Graphics.FillRectangle(track,box);
        int filled=(int)Math.Round((Width-4)*value/100.0);
        if(filled>0) using(var gold=new SolidBrush(Color.FromArgb(230,184,92))) e.Graphics.FillRectangle(gold,2,2,filled,Height-4);
        using(var edge=new Pen(Color.FromArgb(96,130,164))) e.Graphics.DrawRectangle(edge,box);
    }
}

internal sealed class Progress : Form {
    readonly Label caption,percentage,status;
    readonly SetupBar bar;
    readonly SetupProgress progress;
    readonly CancellationTokenSource cancel=new CancellationTokenSource();
    Exception failure;bool done;int shownPercent;
    private Progress(Action<SetupProgress,CancellationToken> action,bool cancellable,Action<SetupUpdate> observer,bool preview=false) {
        Text="Dynasty Warriors: Gundam 2 — Setup";ClientSize=new Size(620,220);
        AutoScaleDimensions=new SizeF(96,96);AutoScaleMode=AutoScaleMode.Dpi;
        StartPosition=FormStartPosition.CenterScreen;FormBorderStyle=FormBorderStyle.FixedDialog;
        MaximizeBox=false;MinimizeBox=false;ControlBox=cancellable;
        BackColor=Color.FromArgb(13,27,44);ForeColor=Color.FromArgb(241,245,250);Font=new Font("Segoe UI",10);
        var title=new Label { Text="DYNASTY WARRIORS: GUNDAM 2",Location=new Point(30,24),Size=new Size(560,26),Font=new Font("Segoe UI",11,FontStyle.Bold),ForeColor=Color.FromArgb(230,184,92) };
        caption=new Label { Text="Installing...",Location=new Point(30,78),Size=new Size(420,32),Font=new Font("Segoe UI",16) };
        percentage=new Label { Text="0%",Location=new Point(476,78),Size=new Size(114,32),Font=new Font("Segoe UI",16),TextAlign=ContentAlignment.TopRight,AccessibleName="Installation percent" };
        bar=new SetupBar { Location=new Point(30,120),Size=new Size(560,30) };
        status=new Label { Text="Preparing setup...",Location=new Point(30,163),Size=new Size(560,36),ForeColor=Color.FromArgb(179,196,213),AutoEllipsis=true };
        Controls.AddRange(new Control[]{title,caption,percentage,bar,status});
        progress=new SetupProgress(delegate(SetupUpdate update) {
            if(observer!=null) observer(update);
            if(!IsDisposed && IsHandleCreated) BeginInvoke((Action)delegate { Apply(update); });
        });
        if(!cancellable) progress.DisableCancellation();
        FormClosing+=delegate(object sender,FormClosingEventArgs e) {
            if(!done) {
                e.Cancel=true;
                if(progress.CanCancel) { cancel.Cancel();caption.Text="Cancelling...";status.Text="Keeping your existing installation safe..."; }
            }
        };
        if(!preview) Shown+=async delegate {
            try {
                await Task.Run(delegate { action(progress,cancel.Token); });
                progress.Complete();Apply(new SetupUpdate(100,"Your game is ready."));
                await Task.Delay(1000); // Keep the successful 100% state visible before continuing.
            } catch(Exception e) { failure=e; }
            done=true;Close();
        };
    }
    void Apply(SetupUpdate update) {
        if(update.Percent<shownPercent) return;
        shownPercent=update.Percent;bar.Value=update.Percent;percentage.Text=update.Percent+"%";status.Text=update.Message;
        if(update.Percent==100) caption.Text="Setup complete";
    }
    internal static void Run(Action<SetupProgress,CancellationToken> action,bool cancellable,Action<SetupUpdate> observer=null) {
        using(var form=new Progress(action,cancellable,observer)) { form.ShowDialog();if(form.failure!=null) throw form.failure; }
    }
    internal static void RenderPreview(string output,int percent) {
        using(var form=new Progress(delegate(SetupProgress p,CancellationToken c){},false,null,true)) {
            form.Show();Application.DoEvents();form.Apply(new SetupUpdate(percent,percent==100?"Your game is ready.":"Preparing your game..."));form.Refresh();
            using(var bitmap=new Bitmap(form.Width,form.Height)) {
                form.DrawToBitmap(bitmap,new Rectangle(Point.Empty,form.Size));bitmap.Save(output,System.Drawing.Imaging.ImageFormat.Png);
            }
            form.done=true;form.Close();
        }
    }
}
