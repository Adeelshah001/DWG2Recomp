using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.IO.Compression;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Web.Script.Serialization;

// Uses only packaged open-source tools. Generated title code never leaves this PC.
internal static class PortableBuild {
    internal sealed class Item { public string path { get; set; } public string sha256 { get; set; } }
    internal sealed class RuntimeItem { public string source { get; set; } public string target { get; set; } }
    internal sealed class Recipe {
        public int format { get; set; }
        public string buildId { get; set; }
        public string compiler { get; set; }
        public string generator { get; set; }
        public string[] compile { get; set; }
        public string[] link { get; set; }
        public Item[] files { get; set; }
        public RuntimeItem[] runtimeFiles { get; set; }
    }
    static readonly string CompressedPackage = Path.Combine(Launcher.Root,"preparation");
    static string Package = CompressedPackage;
    internal sealed class ArchiveInfo {
        public int format { get; set; }
        public string sha256 { get; set; }
        public long expandedBytes { get; set; }
    }
    static string Inside(string root,string relative) {
        if(String.IsNullOrWhiteSpace(relative) || Path.IsPathRooted(relative)) throw new InvalidDataException("Invalid portable package path.");
        string full=Path.GetFullPath(Path.Combine(root,relative.Replace('/',Path.DirectorySeparatorChar)));
        if(!full.StartsWith(Path.GetFullPath(root).TrimEnd('\\')+"\\",StringComparison.OrdinalIgnoreCase)) throw new InvalidDataException("Portable package path escapes its folder.");
        return full;
    }
    static Recipe Load() {
        string file=Path.Combine(Package,"build.json");
        if(!File.Exists(file)) throw new FileNotFoundException("Extract the entire player ZIP beside START_DWG2.exe. Its preparation folder is required.");
        var recipe=new JavaScriptSerializer { MaxJsonLength=16*1024*1024 }.Deserialize<Recipe>(File.ReadAllText(file));
        if(recipe==null || recipe.format!=1 || String.IsNullOrWhiteSpace(recipe.buildId) || recipe.files==null || recipe.compile==null || recipe.link==null || recipe.runtimeFiles==null)
            throw new InvalidDataException("Unsupported portable build package.");
        return recipe;
    }
    internal static bool Ready() {
        string marker=Path.Combine(Launcher.Root,"bin","portable-build.txt");
        if(!File.Exists(marker) || !File.Exists(Path.Combine(Launcher.Root,"bin","dwg2.exe"))) return false;
        var r=Load();string[] state=File.ReadAllLines(marker);
        if(state.Length!=3 || state[0]!=r.buildId || state[1]!=Launcher.XexHash) return false;
        return state[2]==Launcher.Hash(Path.Combine(Launcher.Root,"bin","dwg2.exe")) &&
            r.runtimeFiles.All(f=>File.Exists(Inside(Path.Combine(Launcher.Root,"bin"),f.target)));
    }
    static string Expand(string text,string work) {
        return text.Replace("{portable}",Package).Replace("{generated}",Path.Combine(work,"generated")).Replace('\\','/');
    }
    static string ResponseQuote(string text) { return "\""+text.Replace("\\","\\\\").Replace("\"","\\\"")+"\""; }
    static void Run(string name,string exe,IEnumerable<string> arguments,string work,Action<string> report) {
        report(name);
        string logs=Path.Combine(work,"logs");Directory.CreateDirectory(logs);
        string rsp=Path.Combine(logs,name+".rsp");
        File.WriteAllLines(rsp,arguments.Select(ResponseQuote),new UTF8Encoding(false));
        var info=new ProcessStartInfo(exe,"@"+Launcher.Quote(rsp)) {
            UseShellExecute=false,CreateNoWindow=true,WorkingDirectory=work,RedirectStandardOutput=true,RedirectStandardError=true
        };
        // Do not discover installed compilers, SDKs, Python or Git through PATH/environment.
        info.EnvironmentVariables["PATH"]=Path.GetDirectoryName(exe)+";"+Path.Combine(Package,"runtime")+";"+Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.Windows),"System32");
        foreach(string key in new[]{"INCLUDE","LIB","LIBPATH","CPATH","CPLUS_INCLUDE_PATH","C_INCLUDE_PATH","CL","_CL_","LINK","_LINK_","COMPILER_PATH","GCC_EXEC_PREFIX","SDKROOT"}) info.EnvironmentVariables.Remove(key);
        string temp=Path.Combine(work,"temp");Directory.CreateDirectory(temp);info.EnvironmentVariables["TEMP"]=temp;info.EnvironmentVariables["TMP"]=temp;
        using(var writer=new StreamWriter(Path.Combine(logs,name+".log"),false,new UTF8Encoding(false)))
        using(var child=new Process()) {
            object gate=new object();child.StartInfo=info;
            DataReceivedEventHandler append=delegate(object sender,DataReceivedEventArgs e){if(e.Data!=null) lock(gate) writer.WriteLine(e.Data);};
            child.OutputDataReceived+=append;child.ErrorDataReceived+=append;
            child.Start();child.BeginOutputReadLine();child.BeginErrorReadLine();
            if(!child.WaitForExit(15*60*1000)) { child.Kill();child.WaitForExit();throw new TimeoutException(name+" timed out. Your game and previous build are preserved. See "+logs); }
            child.WaitForExit();
            if(child.ExitCode!=0) throw new InvalidOperationException(name+" failed. Your game and previous build are preserved. See "+Path.Combine(logs,name+".log"));
        }
    }
    internal static void Prepare(Action<string> report) {
        var recipe=Load();
        string archive=Path.Combine(CompressedPackage,"tools.zip");
        string metadata=Path.Combine(CompressedPackage,"package.json");
        if(!File.Exists(archive) || !File.Exists(metadata)) throw new FileNotFoundException("The compressed preparation tools are missing. Extract the complete player ZIP again.");
        var info=new JavaScriptSerializer().Deserialize<ArchiveInfo>(File.ReadAllText(metadata));
        if(info==null || info.format!=1 || info.expandedBytes<0 || Launcher.Hash(archive)!=info.sha256) throw new InvalidDataException("The compressed preparation tools are damaged. Extract the complete player ZIP again.");
        var expected=new Dictionary<string,Item>(StringComparer.OrdinalIgnoreCase);
        foreach(var file in recipe.files) {
            Inside(CompressedPackage,file.path);
            if(expected.ContainsKey(file.path)) throw new InvalidDataException("Duplicate preparation entry.");
            expected.Add(file.path,file);
        }
        expected.Add("build.json",new Item{path="build.json",sha256=Launcher.Hash(Path.Combine(CompressedPackage,"build.json"))});
        string parent=Path.Combine(Launcher.Root,".local-build");Directory.CreateDirectory(parent);
        if((File.GetAttributes(parent)&FileAttributes.ReparsePoint)!=0) throw new IOException("The local preparation folder must not be a link.");
        string temporary=Path.Combine(parent,"tools-"+Guid.NewGuid().ToString("N"));Directory.CreateDirectory(temporary);
        try {
            report("Expanding temporary preparation tools...");
            long total=0;var seen=new HashSet<string>(StringComparer.OrdinalIgnoreCase);
            using(var zip=ZipFile.OpenRead(archive)) {
                foreach(var entry in zip.Entries) {
                    string name=entry.FullName;
                    if(name.Contains("\\") || name.Contains(":") || name.Split('/').Any(x=>x=="" || x=="." || x=="..") || !expected.ContainsKey(name) || !seen.Add(name)) throw new InvalidDataException("Unexpected preparation archive entry.");
                    if(entry.Length<0 || total>info.expandedBytes-entry.Length) throw new InvalidDataException("Preparation archive size differs.");
                    total+=entry.Length;string output=Inside(temporary,name);Directory.CreateDirectory(Path.GetDirectoryName(output));
                    using(var input=entry.Open()) using(var stream=new FileStream(output,FileMode.CreateNew,FileAccess.Write)) input.CopyTo(stream);
                    if(Launcher.Hash(output)!=expected[name].sha256) throw new InvalidDataException("Damaged preparation component: "+name);
                }
            }
            if(total!=info.expandedBytes || seen.Count!=expected.Count) throw new InvalidDataException("Incomplete preparation archive.");
            Package=temporary;
            PrepareExpanded(report);
        } finally {
            Package=CompressedPackage;
            // Only this unique owned tools directory is removed; imported games and saves are outside it.
            try { Directory.Delete(temporary,true); } catch(IOException) {} catch(UnauthorizedAccessException) {}
        }
    }
    static void PrepareExpanded(Action<string> report) {
        var r=Load();report("Checking bundled preparation tools...");
        var seen=new HashSet<string>(StringComparer.OrdinalIgnoreCase);
        foreach(var f in r.files) {
            if(!seen.Add(f.path)) throw new InvalidDataException("Duplicate portable package entry.");
            string path=Inside(Package,f.path);
            if(!File.Exists(path) || Launcher.Hash(path)!=f.sha256) throw new InvalidDataException("The release package is missing or damaged: portable/"+f.path+". Extract the complete ZIP again.");
        }
        string compiler=Inside(Package,r.compiler),generator=Inside(Package,r.generator);
        if(!seen.Contains(r.compiler) || !seen.Contains(r.generator)) throw new InvalidDataException("Unverified preparation tools.");
        foreach(var file in r.runtimeFiles) {
            if(!seen.Contains(file.source) || Path.GetFileName(file.target)!=file.target) throw new InvalidDataException("Unverified runtime payload.");
        }
        string parent=Path.Combine(Launcher.Root,".local-build");Directory.CreateDirectory(parent);
        if((File.GetAttributes(parent)&FileAttributes.ReparsePoint)!=0) throw new IOException("The local preparation folder must not be a link.");
        string work=Path.Combine(parent,"prepare-"+Guid.NewGuid().ToString("N"));Directory.CreateDirectory(work);
        string input=Path.Combine(work,"input");Directory.CreateDirectory(input);
        File.Copy(Path.Combine(Launcher.Game,"default.xex"),Path.Combine(input,"default.xex"));
        string config=Inside(Package,"config/dwg2.toml");
        if(!seen.Contains("config/dwg2.toml")) throw new InvalidDataException("Unverified game configuration.");
        File.Copy(config,Path.Combine(work,"dwg2.toml"));
        // The generator takes one ordinary positional argument, not a compiler response file.
        report("Preparing your game locally...");
        var gen=new ProcessStartInfo(generator,Launcher.Quote(Path.Combine(work,"dwg2.toml"))) {
            UseShellExecute=false,CreateNoWindow=true,WorkingDirectory=work,RedirectStandardOutput=true,RedirectStandardError=true
        };
        gen.EnvironmentVariables["PATH"]=Path.GetDirectoryName(generator)+";"+Path.Combine(Package,"runtime")+";"+Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.Windows),"System32");
        using(var writer=new StreamWriter(Path.Combine(work,"generator.log")))
        using(var child=new Process()) {
            object gate=new object();child.StartInfo=gen;
            DataReceivedEventHandler append=delegate(object sender,DataReceivedEventArgs e){if(e.Data!=null) lock(gate) writer.WriteLine(e.Data);};
            child.OutputDataReceived+=append;child.ErrorDataReceived+=append;child.Start();child.BeginOutputReadLine();child.BeginErrorReadLine();
            if(!child.WaitForExit(15*60*1000)) { child.Kill();child.WaitForExit();throw new TimeoutException("Game preparation timed out. Your import is preserved. See "+work); }
            child.WaitForExit();if(child.ExitCode!=0) throw new InvalidOperationException("Could not prepare this game. Your import is preserved. See "+work);
        }
        string generated=Path.Combine(work,"generated"),objects=Path.Combine(work,"objects");Directory.CreateDirectory(objects);
        string[] sources=Directory.GetFiles(generated,"*.cpp");Array.Sort(sources,StringComparer.Ordinal);
        if(sources.Length<2) throw new InvalidDataException("The generator produced no complete game translation.");
        string pch=Path.Combine(objects,"game.pch");
        var common=r.compile.Select(x=>Expand(x,work)).ToArray();
        Run("prepare-headers",compiler,common.Concat(new[]{"-x","c++-header",Path.Combine(generated,"dwg2_pch.h"),"-o",pch}),work,report);
        var outputs=new string[sources.Length];
        Parallel.For(0,sources.Length,new ParallelOptions{MaxDegreeOfParallelism=2},i=>{
            string output=Path.Combine(objects,i+".obj");outputs[i]=output;
            Run("prepare-part-"+i,compiler,common.Concat(new[]{"-include-pch",pch,"-c",sources[i],"-o",output}),work,report);
        });
        string staged=Path.Combine(work,"bin");Directory.CreateDirectory(staged);
        Run("finish-game",compiler,outputs.Concat(r.link.Select(x=>Expand(x,work))).Concat(new[]{"-o",Path.Combine(staged,"dwg2.exe")}),work,report);
        foreach(var f in r.runtimeFiles) File.Copy(Inside(Package,f.source),Inside(staged,f.target));
        File.WriteAllLines(Path.Combine(staged,"portable-build.txt"),new[]{r.buildId,Launcher.XexHash,Launcher.Hash(Path.Combine(staged,"dwg2.exe"))});
        string bin=Path.Combine(Launcher.Root,"bin"),backup=null;
        if(Directory.Exists(bin)) {
            string backups=Path.Combine(Launcher.Root,"bin-backups");Directory.CreateDirectory(backups);
            backup=Path.Combine(backups,DateTime.UtcNow.ToString("yyyyMMdd-HHmmss")+"-"+Guid.NewGuid().ToString("N"));
            Directory.Move(bin,backup);
        }
        try { Directory.Move(staged,bin); }
        catch { if(backup!=null && !Directory.Exists(bin)) Directory.Move(backup,bin); throw; }
        report("Game prepared. Future launches reuse this executable.");
        // This unique owned workspace contains only transient generated output.
        try { Directory.Delete(work,true); } catch(IOException) {} catch(UnauthorizedAccessException) {}
    }
}
