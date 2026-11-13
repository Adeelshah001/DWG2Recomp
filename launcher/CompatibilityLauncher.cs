using System;using System.IO;using System.Diagnostics;using System.Text;
class CompatibilityLauncher {
static string Quote(string arg){var text=new StringBuilder("\"");int slashes=0;foreach(char c in arg){if(c=='\\'){slashes++;continue;}if(c=='"'){text.Append('\\',slashes*2+1);text.Append(c);}else{text.Append('\\',slashes);text.Append(c);}slashes=0;}text.Append('\\',slashes*2);return text.Append('"').ToString();}
static int Main(string[] args){string root=AppDomain.CurrentDomain.BaseDirectory;string target=Path.Combine(root,"SETUP_DWG2.exe");if(!File.Exists(target))target=Path.Combine(root,"DWG2R.exe");if(!File.Exists(target))return 1;
using(var p=Process.Start(new ProcessStartInfo(target,String.Join(" ",Array.ConvertAll(args,Quote))){WorkingDirectory=root,UseShellExecute=false,CreateNoWindow=true})){p.WaitForExit();return p.ExitCode;}}}
