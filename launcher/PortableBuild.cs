using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.IO.Compression;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Threading;
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
        if(!File.Exists(file)) throw new FileNotFoundException("Extract the entire player ZIP beside DWG2R.exe. Its preparation folder is required.");
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
        report(name=="finish-game"?"Finalizing your game...":name=="prepare-headers"?"Preparing game resources...":"Preparing your game...");
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
    internal static void Prepare(Action<string> report,SetupProgress progress=null) {
        if(progress!=null) progress.Report(0,"Checking setup package...");
        var recipe=Load();
        string archive=Path.Combine(CompressedPackage,"tools.zip");
        string metadata=Path.Combine(CompressedPackage,"package.json");
        if(!File.Exists(archive) || !File.Exists(metadata)) throw new FileNotFoundException("The compressed preparation tools are missing. Extract the complete player ZIP again.");
        var info=new JavaScriptSerializer().Deserialize<ArchiveInfo>(File.ReadAllText(metadata));
        if(info==null || info.format!=1 || info.expandedBytes<0 || Launcher.Hash(archive)!=info.sha256) throw new InvalidDataException("The compressed preparation tools are damaged. Extract the complete player ZIP again.");
        if(progress!=null) progress.Report(0.03,"Expanding temporary preparation tools...");
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
                    string output=Inside(temporary,name);Directory.CreateDirectory(Path.GetDirectoryName(output));
                    using(var input=entry.Open()) using(var stream=new FileStream(output,FileMode.CreateNew,FileAccess.Write)) {
                        byte[] buffer=new byte[1024*1024];int count;
                        while((count=input.Read(buffer,0,buffer.Length))!=0) {
                            stream.Write(buffer,0,count);total+=count;
                            if(progress!=null) progress.Report(0.03+0.17*total/Math.Max(1,info.expandedBytes),"Expanding temporary preparation tools...");
                        }
                    }
                    if(Launcher.Hash(output)!=expected[name].sha256) throw new InvalidDataException("Damaged preparation component: "+name);
                }
            }
            if(total!=info.expandedBytes || seen.Count!=expected.Count) throw new InvalidDataException("Incomplete preparation archive.");
            Package=temporary;
            PrepareExpanded(report,progress==null?null:progress.Range(0.20,0.98));
        } finally {
            Package=CompressedPackage;
            // Only this unique owned tools directory is removed; imported games and saves are outside it.
            try { Directory.Delete(temporary,true); } catch(IOException) {} catch(UnauthorizedAccessException) {}
        }
        if(progress!=null) progress.Report(1,"Preparation finished.");
    }
    static void PrepareExpanded(Action<string> report,SetupProgress progress) {
        var r=Load();report("Checking bundled preparation tools...");
        var seen=new HashSet<string>(StringComparer.OrdinalIgnoreCase);int checkedFiles=0;
        foreach(var f in r.files) {
            if(!seen.Add(f.path)) throw new InvalidDataException("Duplicate portable package entry.");
            string path=Inside(Package,f.path);
            if(!File.Exists(path) || Launcher.Hash(path)!=f.sha256) throw new InvalidDataException("The release package is missing or damaged: "+f.path+". Extract the player ZIP again.");
            checkedFiles++;if(progress!=null) progress.Report(0.04*checkedFiles/Math.Max(1,r.files.Length),"Checking preparation tools...");
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
        string generated=Path.Combine(work,"generated"),objects=Path.Combine(work,"objects");Directory.CreateDirectory(objects);
        using(var cache=new LocalBuildCache(parent,generated,Package,report)) {
        string inputHash=Launcher.Hash(Path.Combine(input,"default.xex"));
        string generationKey=LocalBuildCache.Key(new[]{"generated-source-v1",inputHash}.Concat(r.files.Where(f=>f.path.StartsWith("codegen/",StringComparison.Ordinal)||f.path.StartsWith("config/",StringComparison.Ordinal)||f.path.StartsWith("runtime/",StringComparison.Ordinal)).OrderBy(f=>f.path,StringComparer.Ordinal).Select(f=>f.path+":"+f.sha256)).ToArray());
        bool generationReused=cache.RestoreGeneration(generationKey,generated);
        if(!generationReused) {
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
        cache.StoreGeneration(generationKey,generated);
        } else report("Reusing previously generated game source...");
        if(progress!=null) progress.Report(0.08,"Checking cached game parts...");
        string[] sources=Directory.GetFiles(generated,"*.cpp");Array.Sort(sources,StringComparer.Ordinal);
        if(sources.Length<2)throw new InvalidDataException("The generator produced no complete game translation.");
        string pch=Path.Combine(objects,"game.pch"),pchDependencies=Path.Combine(objects,"headers.d");
        var common=r.compile.Select(x=>Expand(x,work)).Concat(new[]{"-ffile-prefix-map="+work+"=DWG2-local","-ffile-prefix-map="+Package+"=DWG2-preparation"}).ToArray();
        // Dependency scans are cheap; PCH binaries contain temporary paths and are
        // rebuilt only when a game unit actually needs compiling, never reused.
        Run("scan-headers",compiler,common.Concat(new[]{"-M","-MF",pchDependencies,"-MT","headers","-x","c++-header",Path.Combine(generated,"dwg2_pch.h")}),work,report);
        var pchDeps=cache.Dependencies(pchDependencies,null);cache.SharedDependencies(pchDeps);
        string compilerIdentity=LocalBuildCache.Key(r.files.Where(f=>f.path.StartsWith("toolchain/",StringComparison.Ordinal)||f.path=="runtime/libc++.dll"||f.path=="runtime/libunwind.dll"||f.path=="runtime/libwinpthread-1.dll").OrderBy(f=>f.path,StringComparer.Ordinal).Select(f=>f.path+":"+f.sha256).ToArray());
        string headerInventory=LocalBuildCache.Key(r.files.Where(f=>f.path.StartsWith("include/",StringComparison.Ordinal)||f.path.StartsWith("headers/",StringComparison.Ordinal)).Select(f=>f.path).OrderBy(x=>x,StringComparer.Ordinal).Concat(Directory.GetFiles(generated,"*.h").Select(Path.GetFileName).OrderBy(x=>x,StringComparer.Ordinal)).ToArray());
        string context=LocalBuildCache.Key("compile-cache-v1",compilerIdentity,headerInventory,String.Join("\n",r.compile),"stable-prefix-map-v1",LocalBuildCache.DependencyKey(pchDeps));
        var outputs=new string[sources.Length];var keys=new string[sources.Length];var pending=new List<int>();int reused=0;
        for(int i=0;i<sources.Length;++i){keys[i]=LocalBuildCache.Key(context,Path.GetFileName(sources[i]),cache.Hash(sources[i]));if(cache.Reuse(keys[i],out outputs[i]))++reused;else pending.Add(i);}
        report("Build cache: reusing "+reused+" of "+sources.Length+" game parts; compiling "+pending.Count+".");
        if(pending.Count>0)Run("prepare-headers",compiler,common.Concat(new[]{"-x","c++-header",Path.Combine(generated,"dwg2_pch.h"),"-o",pch}),work,report);
        int completed=reused;
        Parallel.ForEach(pending,new ParallelOptions{MaxDegreeOfParallelism=2},i=>{
            string output=Path.Combine(objects,i+".obj"),depfile=Path.Combine(objects,i+".d");outputs[i]=output;
            Run("prepare-part-"+i,compiler,common.Concat(new[]{"-include-pch",pch,"-MD","-MF",depfile,"-MT","unit","-c",sources[i],"-o",output}),work,report);
            cache.Store(keys[i],output,cache.UnitDependencies(cache.Dependencies(depfile,pch)));
            int finished=Interlocked.Increment(ref completed);if(progress!=null)progress.Report(0.18+0.75*finished/sources.Length,"Preparing changed game parts...");
        });
        if(progress!=null)progress.Report(0.94,"Linking updated game...");
        var summary=new {format=1,generationReused=generationReused,reusedUnits=reused,compiledUnits=pending.Count,totalUnits=sources.Length,compileContext=context,rebuiltSources=pending.Select(i=>Path.GetFileName(sources[i])).ToArray()};
        File.WriteAllText(Path.Combine(work,"build-cache-summary.json"),new JavaScriptSerializer().Serialize(summary));
        string cacheLogs=Path.Combine(Launcher.Root,"logs");Directory.CreateDirectory(cacheLogs);
        File.WriteAllText(Path.Combine(cacheLogs,"build-cache-"+DateTime.UtcNow.ToString("yyyyMMdd-HHmmss-fffffff")+".json"),new JavaScriptSerializer().Serialize(summary));
        string staged=Path.Combine(work,"bin");Directory.CreateDirectory(staged);
        Run("finish-game",compiler,outputs.Concat(r.link.Select(x=>Expand(x,work))).Concat(new[]{"-o",Path.Combine(staged,"dwg2.exe")}),work,report);
        if(progress!=null) progress.Report(0.97,"Installing your game executable...");
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
        if(progress!=null) progress.Report(0.99,"Removing temporary preparation files...");
        report("Game prepared. Retained private build cache for future incremental updates.");
        // This unique owned workspace contains only transient generated output.
        try { Directory.Delete(work,true); } catch(IOException) {} catch(UnauthorizedAccessException) {}
        if(progress!=null) progress.Report(1,"Game prepared.");
        } // Cache lease; successful parts survive failed future preparations.
    }
}

// Private, content-checked game build outputs. Never include this cache in releases.
internal sealed class LocalBuildCache : System.IDisposable {
    internal sealed class Dependency { public string path; public string hash; }
    internal sealed class Unit { public int format=1; public string key; public string objectHash; public string seal; public Dependency[] dependencies; }
    internal sealed class GeneratedFile { public string name; public string hash; }
    internal sealed class Generation { public int format=1; public string key; public string seal; public GeneratedFile[] files; }
    readonly string root, generated, portable;
    readonly System.Action<string> report;
    readonly System.IO.FileStream lease;
    readonly System.Collections.Concurrent.ConcurrentDictionary<string,string> hashes=new System.Collections.Concurrent.ConcurrentDictionary<string,string>(System.StringComparer.OrdinalIgnoreCase);
    readonly System.Collections.Concurrent.ConcurrentDictionary<string,string> resolved=new System.Collections.Concurrent.ConcurrentDictionary<string,string>(System.StringComparer.OrdinalIgnoreCase);
    readonly System.Collections.Generic.Dictionary<string,string> shared=new System.Collections.Generic.Dictionary<string,string>(System.StringComparer.OrdinalIgnoreCase);
    internal void SharedDependencies(Dependency[] deps){foreach(var d in deps)shared[d.path]=d.hash;}
    internal Dependency[] UnitDependencies(Dependency[] deps){return deps.Where(d=>!shared.ContainsKey(d.path)).ToArray();}
    static readonly System.Web.Script.Serialization.JavaScriptSerializer json=new System.Web.Script.Serialization.JavaScriptSerializer { MaxJsonLength=16*1024*1024 };
    internal LocalBuildCache(string parent,string generatedRoot,string portableRoot,System.Action<string> log) {
        root=System.IO.Path.Combine(parent,"compile-cache-v1");generated=System.IO.Path.GetFullPath(generatedRoot);portable=System.IO.Path.GetFullPath(portableRoot);report=log;
        Check(parent);System.IO.Directory.CreateDirectory(root);Check(root);
        foreach(string name in new[]{"units","objects","generations","blobs"}) { var p=System.IO.Path.Combine(root,name);System.IO.Directory.CreateDirectory(p);Check(p); }
        var lockPath=System.IO.Path.Combine(root,"cache.lock");Check(lockPath);
        lease=new System.IO.FileStream(lockPath,System.IO.FileMode.OpenOrCreate,System.IO.FileAccess.ReadWrite,System.IO.FileShare.None);
    }
    public void Dispose(){lease.Dispose();}
    static void Check(string path) {
        for(string p=System.IO.Path.GetFullPath(path);p!=null;p=System.IO.Path.GetDirectoryName(p))
            if((System.IO.File.Exists(p)||System.IO.Directory.Exists(p))&&(System.IO.File.GetAttributes(p)&System.IO.FileAttributes.ReparsePoint)!=0)
                throw new System.IO.IOException("Build-cache paths must not be links: "+p);
    }
    static bool Hex(string s){return s!=null&&s.Length==64&&s.All(c=>(c>='0'&&c<='9')||(c>='a'&&c<='f'));}
    internal static string Key(params string[] parts) {
        string input=System.String.Concat(parts.Select(x=>(x??"").Length+":"+(x??"")));
        using(var sha=System.Security.Cryptography.SHA256.Create())return System.BitConverter.ToString(sha.ComputeHash(System.Text.Encoding.UTF8.GetBytes(input))).Replace("-","").ToLowerInvariant();
    }
    internal string Hash(string path){return hashes.GetOrAdd(System.IO.Path.GetFullPath(path),p=>Launcher.Hash(p));}
    string At(string folder,string key,string suffix){if(!Hex(key))throw new System.IO.InvalidDataException("Invalid build cache key.");string p=System.IO.Path.Combine(root,folder,key+suffix);Check(p);return p;}
    static void Atomic(string destination,System.Action<string> write) {
        Check(destination);string temp=System.IO.Path.Combine(System.IO.Path.GetDirectoryName(destination),"."+System.Guid.NewGuid().ToString("N")+".tmp");
        try { write(temp);if(System.IO.File.Exists(destination))System.IO.File.Replace(temp,destination,null);else System.IO.File.Move(temp,destination); }
        finally {if(System.IO.File.Exists(temp))System.IO.File.Delete(temp);}
    }
    string Blob(string folder,string source,string suffix) {
        string digest=Launcher.Hash(source),destination=At(folder,digest,suffix);
        if(!System.IO.File.Exists(destination)||Launcher.Hash(destination)!=digest) {
            try { Atomic(destination,temp=>{System.IO.File.Copy(source,temp);if(Launcher.Hash(temp)!=digest)throw new System.IO.IOException("Build cache copy changed.");}); }
            catch(System.IO.IOException){if(!System.IO.File.Exists(destination)||Launcher.Hash(destination)!=digest)throw;}
        }
        return digest;
    }
    static bool CacheFault(System.Exception e){return e is System.IO.IOException||e is System.UnauthorizedAccessException||e is System.ArgumentException||e is System.InvalidOperationException;}
    static bool FileName(string n){return !System.String.IsNullOrEmpty(n)&&n==System.IO.Path.GetFileName(n)&&n.IndexOfAny(new[]{'\\','/',':'})<0;}
    internal bool RestoreGeneration(string key,string destination) {
        bool copying=false;
        try {
            string index=At("generations",key,".json");if(!System.IO.File.Exists(index))return false;
            var entry=json.Deserialize<Generation>(System.IO.File.ReadAllText(index));
            if(entry==null||entry.format!=1||entry.key!=key||entry.files==null||entry.files.Length<3)return false;
            var names=new System.Collections.Generic.HashSet<string>(System.StringComparer.OrdinalIgnoreCase);
            foreach(var f in entry.files) {
                if(f==null||!FileName(f.name)||!(f.name.EndsWith(".cpp")||f.name.EndsWith(".h"))||!names.Add(f.name)||!Hex(f.hash))return false;
                string blob=At("blobs",f.hash,".blob");if(!System.IO.File.Exists(blob)||Launcher.Hash(blob)!=f.hash)return false;
            }
            if(entry.seal!=Key(new[]{"generation-v1",key}.Concat(entry.files.Select(f=>f.name+":"+f.hash)).ToArray()))return false;
            if(!names.Contains("dwg2_pch.h")||entry.files.Count(x=>x.name.EndsWith(".cpp"))<2)return false;
            System.IO.Directory.CreateDirectory(destination);
            // Validate every blob before copying anything into the new build workspace.
            copying=true;
            foreach(var f in entry.files)System.IO.File.Copy(At("blobs",f.hash,".blob"),System.IO.Path.Combine(destination,f.name),false);
            return true;
        } catch(System.Exception e) {if(copying||!CacheFault(e))throw;report("Generated-source cache unavailable; preparing fresh source.");return false;}
    }
    internal void StoreGeneration(string key,string directory) {
        try {
            var files=System.IO.Directory.GetFiles(directory).Where(p=>p.EndsWith(".cpp")||p.EndsWith(".h")).OrderBy(p=>p,System.StringComparer.Ordinal).Select(p=>new GeneratedFile{name=System.IO.Path.GetFileName(p),hash=Blob("blobs",p,".blob")}).ToArray();
            var entry=new Generation{key=key,files=files,seal=Key(new[]{"generation-v1",key}.Concat(files.Select(f=>f.name+":"+f.hash)).ToArray())};Atomic(At("generations",key,".json"),p=>System.IO.File.WriteAllText(p,json.Serialize(entry),new System.Text.UTF8Encoding(false)));
        } catch(System.Exception e){if(!CacheFault(e))throw;report("Could not retain generated-source cache; the game build can continue.");}
    }
    string Resolve(string name) {
        string existing;if(name!=null&&resolved.TryGetValue(name,out existing))return existing;
        if(System.String.IsNullOrEmpty(name))throw new System.IO.InvalidDataException("Empty cache dependency.");
        string basePath,relative;
        if(name.StartsWith("G/")){basePath=generated;relative=name.Substring(2);}else if(name.StartsWith("P/")){basePath=portable;relative=name.Substring(2);}else throw new System.IO.InvalidDataException("Unknown cache dependency root.");
        if(relative.Length==0||relative.Contains(":")||relative.Contains("\\")||relative.Split('/').Any(x=>x==""||x=="."||x==".."))throw new System.IO.InvalidDataException("Unsafe cache dependency.");
        string path=System.IO.Path.GetFullPath(System.IO.Path.Combine(basePath,relative.Replace('/',System.IO.Path.DirectorySeparatorChar)));Check(path);resolved.TryAdd(name,path);return path;
    }
    string Name(string path) {
        path=System.IO.Path.GetFullPath(path);
        foreach(var pair in new[]{new[]{generated,"G/"},new[]{portable,"P/"}})
            if(path.StartsWith(pair[0].TrimEnd('\\')+"\\",System.StringComparison.OrdinalIgnoreCase))return pair[1]+path.Substring(pair[0].TrimEnd('\\').Length+1).Replace('\\','/');
        throw new System.IO.InvalidDataException("Untracked compiler dependency outside bundled/generated files: "+path);
    }
    internal Dependency[] Dependencies(string depfile,string pch) {
        string text=System.IO.File.ReadAllText(depfile).Replace("\\\r\n","").Replace("\\\n","");int colon=text.IndexOf(':');if(colon<0)throw new System.IO.InvalidDataException("Invalid compiler dependency file.");
        var paths=new System.Collections.Generic.List<string>();var token=new System.Text.StringBuilder();
        for(int i=colon+1;i<=text.Length;++i) {
            char c=i==text.Length?' ':text[i];
            if(c=='\\'&&i+1<text.Length&&(System.Char.IsWhiteSpace(text[i+1])||text[i+1]=='\\'||text[i+1]=='#'||text[i+1]==':'||text[i+1]=='$')){token.Append(text[++i]);continue;}
            if(c=='$'&&i+1<text.Length&&text[i+1]=='$'){token.Append('$');++i;continue;}
            if(System.Char.IsWhiteSpace(c)){if(token.Length>0){paths.Add(token.ToString());token.Clear();}}else token.Append(c);
        }
        var names=new System.Collections.Generic.HashSet<string>(System.StringComparer.OrdinalIgnoreCase);
        foreach(var path in paths) {var full=System.IO.Path.GetFullPath(path);if(pch!=null&&System.String.Equals(full,System.IO.Path.GetFullPath(pch),System.StringComparison.OrdinalIgnoreCase))continue;names.Add(Name(full));}
        if(names.Count==0)throw new System.IO.InvalidDataException("Compiler reported no tracked dependencies.");
        return names.OrderBy(x=>x,System.StringComparer.Ordinal).Select(n=>new Dependency{path=n,hash=Hash(Resolve(n))}).ToArray();
    }
    internal static string DependencyKey(Dependency[] deps){return Key(deps.Select(x=>x.path+":"+x.hash).ToArray());}
    internal bool Reuse(string key,out string output) {
        output=null;
        try {
            string index=At("units",key,".json");if(!System.IO.File.Exists(index))return false;
            Unit entry;lock(json)entry=json.Deserialize<Unit>(System.IO.File.ReadAllText(index));
            if(entry==null||entry.format!=1||entry.key!=key||!Hex(entry.objectHash)||entry.dependencies==null||entry.dependencies.Length==0)return false;
            if(entry.dependencies.Any(d=>d==null||System.String.IsNullOrEmpty(d.path)||!Hex(d.hash))||entry.seal!=Key("unit-v1",key,entry.objectHash,DependencyKey(entry.dependencies)))return false;
            foreach(var d in entry.dependencies) {
                string known;if(shared.TryGetValue(d.path,out known)){if(known!=d.hash)return false;continue;}
                string path=Resolve(d.path);if(!System.IO.File.Exists(path)||Hash(path)!=d.hash)return false;
            }
            string obj=At("objects",entry.objectHash,".obj");if(!System.IO.File.Exists(obj)||Launcher.Hash(obj)!=entry.objectHash)return false;
            output=obj;return true;
        } catch(System.Exception e){if(!CacheFault(e))throw;report("Ignoring an invalid cached game part; it will be rebuilt.");return false;}
    }
    internal void Store(string key,string output,Dependency[] dependencies) {
        try {
            var entry=new Unit{key=key,objectHash=Blob("objects",output,".obj"),dependencies=dependencies};entry.seal=Key("unit-v1",key,entry.objectHash,DependencyKey(dependencies));string text;lock(json)text=json.Serialize(entry);
            Atomic(At("units",key,".json"),p=>System.IO.File.WriteAllText(p,text,new System.Text.UTF8Encoding(false)));
        } catch(System.Exception e){if(!CacheFault(e))throw;report("Could not cache a prepared game part; the completed build can still be installed.");}
    }
}
