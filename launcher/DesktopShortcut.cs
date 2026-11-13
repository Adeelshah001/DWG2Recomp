using System;
using System.IO;
using System.Runtime.InteropServices;
using System.Runtime.InteropServices.ComTypes;
using System.Text;

internal static class DesktopShortcut {
    internal const string Title="DWG2R";
    internal const string Description="Play Dynasty Warriors: Gundam 2";
    const string Marker="desktop-shortcut.txt";

    // Discover actual links; a stale marker must not create duplicates.
    // Existing links are never renamed or rewritten.
    internal static void TryEnsure(string root) {
        try { Ensure(root,Environment.GetFolderPath(Environment.SpecialFolder.DesktopDirectory)); }
        catch(Exception e) {
            if(!(e is IOException || e is UnauthorizedAccessException || e is COMException || e is ArgumentException)) throw;
            try { Directory.CreateDirectory(Path.Combine(root,"logs"));File.WriteAllText(Path.Combine(root,"logs","desktop-shortcut.log"),e.ToString()); } catch { }
        }
    }

    internal static string Ensure(string root,string desktop) {
        root=Path.GetFullPath(root);desktop=Path.GetFullPath(desktop);
        string target=Path.Combine(root,File.Exists(Path.Combine(root,"DWG2R.exe"))?"DWG2R.exe":"SETUP_DWG2.exe"),legacy=Path.Combine(root,"START_DWG2.exe");
        if(!File.Exists(target))throw new FileNotFoundException("The game launcher is missing.",target);
        if(!Directory.Exists(desktop))throw new DirectoryNotFoundException("The current user's desktop is unavailable.");
        string marker=Path.Combine(root,Marker),existing=null;
        // Prefer the canonical name; otherwise reuse an existing link anywhere
        // on this desktop, even if a prior installer marker is absent or stale.
        var links=Directory.GetFiles(desktop,"*.lnk");Array.Sort(links,StringComparer.OrdinalIgnoreCase);
        foreach(string path in links) {
            if(!TargetsInstallation(path,target,legacy)&&!TargetsInstallation(path,Path.Combine(root,"SETUP_DWG2.exe"),legacy))continue;
            if(existing==null)existing=path;
            if(String.Equals(Path.GetFileName(path),Title+".lnk",StringComparison.OrdinalIgnoreCase)){existing=path;break;}
        }
        // Existing links keep their name, target, icon and custom arguments.
        // Legacy targets remain supported by the installer's forwarding EXE.
        if(existing!=null)return existing;
        string destination=null;
        for(int suffix=0;suffix<100;suffix++) {
            string path=Path.Combine(desktop,Title+(suffix==0?"":" ("+(suffix+1)+")")+".lnk");
            if(!File.Exists(path)&&!Directory.Exists(path)){destination=path;break;}
        }
        if(destination==null)throw new IOException("No unused game shortcut name is available on the desktop.");
        WriteLink(destination,target,false);
        File.WriteAllLines(marker,new[]{target,destination});return destination;
    }

    static bool TargetsInstallation(string path,string target,string legacy) {
        if((File.GetAttributes(path)&FileAttributes.ReparsePoint)!=0)return false;
        object com=null;
        try {
            com=new ShellLink();((IPersistFile)com).Load(path,0);var link=(IShellLinkW)com;
            var value=new StringBuilder(32768);link.GetPath(value,value.Capacity,IntPtr.Zero,4);
            string found=Path.GetFullPath(Environment.ExpandEnvironmentVariables(value.ToString()));
            return String.Equals(found,target,StringComparison.OrdinalIgnoreCase)||String.Equals(found,legacy,StringComparison.OrdinalIgnoreCase);
        } catch(COMException){return false;}catch(ArgumentException){return false;}
        finally{if(com!=null)Marshal.FinalReleaseComObject(com);}
    }

    [DllImport("shell32.dll",CharSet=CharSet.Unicode)]static extern void SHChangeNotify(uint change,uint flags,string item1,IntPtr item2);
    static void NotifyShortcut(string path){SHChangeNotify(0x00002000,0x0005,path,IntPtr.Zero);}

    internal static void WriteLink(string path,string target,bool replace) {
            string temporary=Path.Combine(Path.GetDirectoryName(path),".dwg2-"+Guid.NewGuid().ToString("N")+".lnk");
            object com=null;
            try {
                com=new ShellLink();var link=(IShellLinkW)com;
                link.SetPath(target);link.SetWorkingDirectory(Path.GetDirectoryName(target));
                link.SetArguments("");link.SetDescription(Description);link.SetIconLocation(target,0);link.SetShowCmd(1);
                ((IPersistFile)com).Save(temporary,true);
                if(replace)File.Replace(temporary,path,null);
                else File.Move(temporary,path);
                NotifyShortcut(path);
            } finally {
                if(com!=null)Marshal.FinalReleaseComObject(com);
                if(File.Exists(temporary))File.Delete(temporary);
            }
    }

    internal static bool Matches(string path,string target) {return MatchesBranding(path,target,false);}
    static bool MatchesBranding(string path,string target,bool preserveArguments) {
        object com=null;
        try {
            com=new ShellLink();((IPersistFile)com).Load(path,0);var link=(IShellLinkW)com;
            var value=new StringBuilder(32768);link.GetPath(value,value.Capacity,IntPtr.Zero,4);
            if(!String.Equals(value.ToString(),target,StringComparison.OrdinalIgnoreCase))return false;
            link.GetDescription(value.Clear(),value.Capacity);if(value.ToString()!=Description)return false;
            link.GetArguments(value.Clear(),value.Capacity);if(value.Length!=0 && !preserveArguments)return false;
            int index;link.GetIconLocation(value.Clear(),value.Capacity,out index);
            if(index!=0 || !String.Equals(value.ToString(),target,StringComparison.OrdinalIgnoreCase))return false;
            link.GetWorkingDirectory(value.Clear(),value.Capacity);
            return String.Equals(value.ToString(),Path.GetDirectoryName(target),StringComparison.OrdinalIgnoreCase);
        } catch(COMException) { return false; }
        finally { if(com!=null)Marshal.FinalReleaseComObject(com); }
    }

    [ComImport,Guid("00021401-0000-0000-C000-000000000046")] sealed class ShellLink { }
    [ComImport,Guid("000214F9-0000-0000-C000-000000000046"),InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
    interface IShellLinkW {
        void GetPath([Out,MarshalAs(UnmanagedType.LPWStr)]StringBuilder path,int count,IntPtr findData,uint flags);
        void GetIDList(out IntPtr list);void SetIDList(IntPtr list);
        void GetDescription([Out,MarshalAs(UnmanagedType.LPWStr)]StringBuilder value,int count);
        void SetDescription([MarshalAs(UnmanagedType.LPWStr)]string value);
        void GetWorkingDirectory([Out,MarshalAs(UnmanagedType.LPWStr)]StringBuilder value,int count);
        void SetWorkingDirectory([MarshalAs(UnmanagedType.LPWStr)]string value);
        void GetArguments([Out,MarshalAs(UnmanagedType.LPWStr)]StringBuilder value,int count);
        void SetArguments([MarshalAs(UnmanagedType.LPWStr)]string value);
        void GetHotkey(out short key);void SetHotkey(short key);void GetShowCmd(out int command);void SetShowCmd(int command);
        void GetIconLocation([Out,MarshalAs(UnmanagedType.LPWStr)]StringBuilder value,int count,out int index);
        void SetIconLocation([MarshalAs(UnmanagedType.LPWStr)]string value,int index);
        void SetRelativePath([MarshalAs(UnmanagedType.LPWStr)]string value,uint reserved);
        void Resolve(IntPtr window,uint flags);void SetPath([MarshalAs(UnmanagedType.LPWStr)]string path);
    }
}
