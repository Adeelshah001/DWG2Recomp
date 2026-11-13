using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Reflection;
using System.Text;
using System.Threading;
// One editable player file; legacy files are migrated once and retained privately.
internal static class PlayerSettings {
 internal static readonly string[] Profiles={"controls.keyboard","controls.playstation","controls.xbox"};
 internal static readonly string[] Actions={"a","b","x","y","lb","rb","lt","rt","back","start","l3","r3","up","down","left","right","left_stick_up","left_stick_down","left_stick_left","left_stick_right","right_stick_up","right_stick_down","right_stick_left","right_stick_right","fullscreen"};
 internal sealed class Ini {
  internal List<string> Lines=new List<string>();internal Dictionary<string,string> Values=new Dictionary<string,string>(StringComparer.OrdinalIgnoreCase);
  internal Ini(string text){if(text.Length>1024*1024)throw new InvalidDataException("settings.ini is too large.");string section="";foreach(string raw in text.TrimStart('\uFEFF').Replace("\r\n","\n").Split('\n')){Lines.Add(raw);string s=raw.Trim();if(s.Length==0||s[0]==';'||s[0]=='#')continue;if(s[0]=='['&&s.EndsWith("]")){section=s.Substring(1,s.Length-2).Trim();if(section.Length==0)throw new InvalidDataException("Empty settings section.");continue;}int eq=s.IndexOf('=');if(eq<=0||section.Length==0)throw new InvalidDataException("Invalid settings.ini line: "+raw);string key=section+"/"+s.Substring(0,eq).Trim(),value=s.Substring(eq+1);int comment=value.IndexOfAny(new[]{';','#'});if(comment>=0)value=value.Substring(0,comment);if(Values.ContainsKey(key))throw new InvalidDataException("Duplicate settings.ini key: "+key);Values.Add(key,value.Trim());}}
  internal string Get(string section,string key,string fallback){string v;return Values.TryGetValue(section+"/"+key,out v)?v:fallback;}
  internal void Set(string section,string key,string value){string current="";int insert=Lines.Count;bool found=false;for(int i=0;i<Lines.Count;i++){string s=Lines[i].Trim();if(s.StartsWith("[")&&s.EndsWith("]")){if(found){insert=i;break;}current=s.Substring(1,s.Length-2).Trim();if(current.Equals(section,StringComparison.OrdinalIgnoreCase)){found=true;insert=i+1;}continue;}if(current.Equals(section,StringComparison.OrdinalIgnoreCase)){insert=i+1;int eq=s.IndexOf('=');if(eq>0&&s.Substring(0,eq).Trim().Equals(key,StringComparison.OrdinalIgnoreCase)){int c=s.IndexOfAny(new[]{';','#'},eq+1);Lines[i]=key+" = "+value+(c<0?"":" "+s.Substring(c));Values[section+"/"+key]=value;return;}}}if(!found){Lines.Add("");Lines.Add("["+section+"]");insert=Lines.Count;}Lines.Insert(insert,key+" = "+value);Values[section+"/"+key]=value;}
  public override string ToString(){return String.Join("\n",Lines).TrimEnd('\n')+"\n";}
 }
 internal static int Number(string text,int min,int max,string key){int n;if(!Int32.TryParse(text,out n)||n<min||n>max)throw new InvalidDataException("Invalid "+key+" in settings.ini.");return n;}
 internal static Ini Load(string root){return new Ini(File.ReadAllText(Path.Combine(root,"settings.ini")));}
 internal static int Integer(string root,string section,string key,int fallback,int min,int max){var doc=Load(root);return Number(doc.Get(section,key,fallback.ToString()),min,max,section+"/"+key);}
 internal static void Validate(Ini doc){Number(doc.Get("graphics","resolution_scale","1"),1,3,"resolution_scale");int f=Number(doc.Get("camera","fov_percent","0"),0,50,"fov_percent");if(f%5!=0)throw new InvalidDataException("fov_percent must be a multiple of 5.");Number(doc.Get("video","vsync","0"),0,1,"vsync");Number(doc.Get("video","fullscreen","0"),0,1,"fullscreen");Number(doc.Get("prompts","style","0"),0,2,"style");Number(doc.Get("prompts","hud_correction","1"),0,1,"hud_correction");for(int type=0;type<3;type++){Number(doc.Get(Profiles[type],"deadzone","0"),0,50,"deadzone");foreach(string action in Actions)foreach(string suffix in new[]{"",".alternate"}){string value=doc.Get(Profiles[type],action+suffix,"0,0,1,0");var v=value.Split(',').Select(x=>Number(x,Int32.MinValue,Int32.MaxValue,action)).ToArray();if(v.Length!=4||v[0]<0||v[0]>3||(v[0]!=0&&(v[3]<0||v[3]>7||(v[2]!=1&&v[2]!=-1)|| (type==0?(v[0]!=1||v[1]<=0||v[1]>=256):((v[0]!=2||v[1]<0||v[1]>=26)&&(v[0]!=3||v[1]<0||v[1]>=6)||v[3]!=0)))))throw new InvalidDataException("Invalid control binding: "+Profiles[type]+"/"+action+suffix);}}}
 internal static void Ensure(string root){string path=Path.Combine(root,"settings.ini");if(File.Exists(path)){var existing=Load(root);Validate(existing);UpgradeShortcuts(root,existing);ArchiveLegacy(root);return;}Ini doc;using(var stream=Assembly.GetExecutingAssembly().GetManifestResourceStream("SettingsDefaults.ini")){if(stream==null)throw new InvalidDataException("Default settings resource is missing.");using(var reader=new StreamReader(stream))doc=new Ini(reader.ReadToEnd());}
  string old=Path.Combine(root,"display.ini");if(File.Exists(old))doc.Set("graphics","resolution_scale",File.ReadAllText(old).Trim());
  old=Path.Combine(root,"camera.ini");if(File.Exists(old)){var v=File.ReadAllText(old).Split((char[])null,StringSplitOptions.RemoveEmptyEntries);if(v.Length==2&&v[0]=="DWG2_FOV_V2")doc.Set("camera","fov_percent",v[1]);else if(v.Length==1&&(v[0]=="0"||v[0]=="1"))doc.Set("camera","fov_percent",v[0]=="1"?"10":"0");else throw new InvalidDataException("Invalid legacy camera.ini; original preserved.");}
  old=Path.Combine(root,"video.ini");if(File.Exists(old)){var v=File.ReadAllText(old).Split((char[])null,StringSplitOptions.RemoveEmptyEntries);if(v.Length!=2||v[0]!="DWG2_VIDEO_V1")throw new InvalidDataException("Invalid legacy video.ini; original preserved.");doc.Set("video","vsync",v[1]);}
  old=Path.Combine(root,"prompts.ini");if(File.Exists(old)){var v=File.ReadAllText(old).Split((char[])null,StringSplitOptions.RemoveEmptyEntries);if(v.Length!=2)throw new InvalidDataException("Invalid legacy prompts.ini; original preserved.");doc.Set("prompts","style",v[0]);doc.Set("prompts","hud_correction",v[1]);}
  old=Path.Combine(root,"controls.ini");if(File.Exists(old)){var v=File.ReadAllText(old).Split((char[])null,StringSplitOptions.RemoveEmptyEntries);if(v.Length!=605||v[0]!="DWG2_INPUT"||v[1]!="1")throw new InvalidDataException("Invalid legacy controls.ini; original preserved.");int index=2;foreach(string profile in Profiles){doc.Set(profile,"deadzone",v[index++]);foreach(string action in Actions)foreach(string suffix in new[]{"",".alternate"}){doc.Set(profile,action+suffix,String.Join(",",v.Skip(index).Take(4)));index+=4;}}}
  Validate(doc);string temp=path+"."+System.Diagnostics.Process.GetCurrentProcess().Id+".tmp";File.WriteAllText(temp,doc.ToString(),new UTF8Encoding(false));try{File.Move(temp,path);}catch{if(File.Exists(temp))File.Delete(temp);throw;}
  ArchiveLegacy(root);
 }
 internal static void UpgradeShortcuts(string root,Ini doc) {
  if(doc.Get("controls","fullscreen_shortcut_schema","0")=="2")return;
  string keyboard="controls.keyboard",ps="controls.playstation",xbox="controls.xbox";
  if(doc.Get(keyboard,"fullscreen","")=="1,13,1,4"&&doc.Get(keyboard,"fullscreen.alternate","0,0,1,0")=="0,0,1,0")doc.Set(keyboard,"fullscreen.alternate","1,122,1,0");
  if(doc.Get(ps,"fullscreen","")=="2,20,1,0"&&doc.Get(ps,"fullscreen.alternate","0,0,1,0")=="0,0,1,0"){doc.Set(ps,"fullscreen","3,4,1,0");doc.Set(ps,"fullscreen.alternate","2,20,1,0");}
  if(doc.Get(xbox,"fullscreen","0,0,1,0")=="0,0,1,0")doc.Set(xbox,"fullscreen","3,4,1,0");
  if(!doc.Values.ContainsKey("video/fullscreen"))doc.Set("video","fullscreen","0");
  doc.Set("controls","fullscreen_shortcut_schema","2");Validate(doc);
  string path=Path.Combine(root,"settings.ini"),temp=path+"."+Guid.NewGuid().ToString("N")+".tmp",backup=Path.Combine(root,"user-data","settings-backup",DateTime.UtcNow.ToString("yyyyMMdd-HHmmss")+"-shortcuts-"+Guid.NewGuid().ToString("N"));
  Directory.CreateDirectory(backup);File.WriteAllText(temp,doc.ToString(),new UTF8Encoding(false));
  try {File.Replace(temp,path,Path.Combine(backup,"settings.ini"));}finally{if(File.Exists(temp))File.Delete(temp);}
 }
 internal static void ArchiveLegacy(string root){
  string backup=Path.Combine(root,"user-data","settings-backup",DateTime.UtcNow.ToString("yyyyMMdd-HHmmss")+"-"+Guid.NewGuid().ToString("N"));var names=new[]{"display.ini","camera.ini","video.ini","prompts.ini","controls.ini"};if(names.Any(n=>File.Exists(Path.Combine(root,n)))){Directory.CreateDirectory(backup);foreach(string name in names){string file=Path.Combine(root,name);if(File.Exists(file))File.Move(file,Path.Combine(backup,name));}}
 }
}
