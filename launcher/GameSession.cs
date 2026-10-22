using System;
using System.Diagnostics;
using System.IO;
using System.Threading;

// A fresh process is essential: the guest, audio and renderer own process-wide
// state. The event is private to this launcher session and never touches saves.
internal static class GameSession {
    internal const string RestartVariable="DWG2_RESTART_EVENT";
    internal static void Run(ProcessStartInfo info,TextWriter log) {
        string name="Local\\DWG2-Restart-"+Guid.NewGuid().ToString("N");
        using(var restart=new EventWaitHandle(false,EventResetMode.ManualReset,name)) {
            info.EnvironmentVariables[RestartVariable]=name;
            do {
                restart.Reset();
                using(var p=new Process()) {
                    object gate=new object();p.StartInfo=info;
                    DataReceivedEventHandler record=delegate(object sender,DataReceivedEventArgs e) {
                        if(e.Data!=null)lock(gate){log.WriteLine(e.Data);log.Flush();}
                    };
                    p.OutputDataReceived+=record;p.ErrorDataReceived+=record;
                    p.Start();p.BeginOutputReadLine();p.BeginErrorReadLine();p.WaitForExit();
                    if(p.ExitCode!=0)throw new InvalidOperationException(
                        "The game exited unexpectedly ("+p.ExitCode+", 0x"+
                        unchecked((uint)p.ExitCode).ToString("X8")+"). See the logs folder.");
                }
            } while(restart.WaitOne(0));
        }
    }
}
