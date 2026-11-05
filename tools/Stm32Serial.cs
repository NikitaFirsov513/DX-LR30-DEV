// STM32 USART bootloader (ST AN3155). Windows COM transport; no option-byte commands.
using System;
using System.IO;
using System.Text;
using System.Threading;
using System.Globalization;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using System.ComponentModel;
using Microsoft.Win32.SafeHandles;

namespace Pj26Flash {
 public sealed class SerialDevice {
  public string Port, Name, HardwareId, InstanceId;
 }
 public static class SerialDiscovery {
  [StructLayout(LayoutKind.Sequential)] struct DeviceInfo {
   public uint Size; public Guid ClassGuid; public uint Instance; public UIntPtr Reserved;
  }
  [DllImport("setupapi.dll",CharSet=CharSet.Unicode,SetLastError=true)]
  static extern IntPtr SetupDiGetClassDevs(ref Guid g,string e,IntPtr w,uint f);
  [DllImport("setupapi.dll",SetLastError=true)] static extern bool SetupDiEnumDeviceInfo(IntPtr set,uint index,ref DeviceInfo info);
  [DllImport("setupapi.dll",CharSet=CharSet.Unicode,SetLastError=true)]
  static extern bool SetupDiGetDeviceRegistryProperty(IntPtr set,ref DeviceInfo info,uint property,out uint type,byte[] data,uint size,out uint needed);
  [DllImport("setupapi.dll",CharSet=CharSet.Unicode,SetLastError=true)]
  static extern bool SetupDiGetDeviceInstanceId(IntPtr set,ref DeviceInfo info,StringBuilder id,int size,out int needed);
  [DllImport("setupapi.dll",SetLastError=true)] static extern IntPtr SetupDiOpenDevRegKey(IntPtr set,ref DeviceInfo info,uint scope,uint profile,uint key,uint access);
  [DllImport("setupapi.dll")] static extern bool SetupDiDestroyDeviceInfoList(IntPtr set);
  [DllImport("advapi32.dll",CharSet=CharSet.Unicode)] static extern int RegQueryValueEx(IntPtr key,string name,IntPtr reserved,out uint type,byte[] data,ref uint size);
  [DllImport("advapi32.dll")] static extern int RegCloseKey(IntPtr key);
  static string Property(IntPtr set,ref DeviceInfo info,uint property) {
   byte[] data=new byte[4096];uint type,needed;
   if(!SetupDiGetDeviceRegistryProperty(set,ref info,property,out type,data,(uint)data.Length,out needed))return "";
   return Encoding.Unicode.GetString(data,0,(int)Math.Min(needed,(uint)data.Length)).TrimEnd('\0').Replace('\0',';');
  }
  public static SerialDevice[] Devices() {
   Guid ports=new Guid("4d36e978-e325-11ce-bfc1-08002be10318");
   IntPtr set=SetupDiGetClassDevs(ref ports,null,IntPtr.Zero,2);
   if(set==new IntPtr(-1))throw new Win32Exception(Marshal.GetLastWin32Error());
   var result=new List<SerialDevice>();
   try {
    for(uint index=0;;index++) {
     DeviceInfo info=new DeviceInfo();info.Size=(uint)Marshal.SizeOf(typeof(DeviceInfo));
     if(!SetupDiEnumDeviceInfo(set,index,ref info)) {
      int error=Marshal.GetLastWin32Error();if(error==259)break;throw new Win32Exception(error);
     }
     string port="";IntPtr key=SetupDiOpenDevRegKey(set,ref info,1,0,1,0x20019);
     if(key!=new IntPtr(-1))try {
      byte[] data=new byte[256];uint type,size=(uint)data.Length;
      if(RegQueryValueEx(key,"PortName",IntPtr.Zero,out type,data,ref size)==0)
       port=Encoding.Unicode.GetString(data,0,(int)size).TrimEnd('\0');
     } finally {RegCloseKey(key);}
     if(String.IsNullOrEmpty(port))continue;
     var id=new StringBuilder(1024);int required;
     SetupDiGetDeviceInstanceId(set,ref info,id,id.Capacity,out required);
     string name=Property(set,ref info,12);if(name.Length==0)name=Property(set,ref info,0);
     result.Add(new SerialDevice{Port=port,Name=name,HardwareId=Property(set,ref info,1),InstanceId=id.ToString()});
    }
   } finally {SetupDiDestroyDeviceInfoList(set);}
   return result.ToArray();
  }
 }
 public interface IWire : IDisposable {
  void Write(byte[] data);
  byte[] Read(int count, int timeoutMs);
  void Lines(bool rts, bool dtr);
  void Purge();
 }

 public sealed class WinCom : IWire {
  [StructLayout(LayoutKind.Sequential)] struct Dcb {
   public uint Length, Baud, Flags; public ushort Reserved, XonLim, XoffLim;
   public byte ByteSize, Parity, StopBits, XonChar, XoffChar, ErrorChar, EofChar, EvtChar;
   public ushort Reserved1;
  }
  [StructLayout(LayoutKind.Sequential)] struct Timeouts {
   public uint Interval, ReadMult, ReadConst, WriteMult, WriteConst;
  }
  [DllImport("kernel32.dll", CharSet=CharSet.Unicode, SetLastError=true)]
  static extern SafeFileHandle CreateFile(string n,uint a,uint s,IntPtr sa,uint c,uint f,IntPtr t);
  [DllImport("kernel32.dll",SetLastError=true)] static extern bool GetCommState(SafeFileHandle h,ref Dcb d);
  [DllImport("kernel32.dll",SetLastError=true)] static extern bool SetCommState(SafeFileHandle h,ref Dcb d);
  [DllImport("kernel32.dll",SetLastError=true)] static extern bool SetCommTimeouts(SafeFileHandle h,ref Timeouts t);
  [DllImport("kernel32.dll",SetLastError=true)] static extern bool EscapeCommFunction(SafeFileHandle h,uint f);
  [DllImport("kernel32.dll",SetLastError=true)] static extern bool PurgeComm(SafeFileHandle h,uint f);
  [DllImport("kernel32.dll",SetLastError=true)] static extern bool WriteFile(SafeFileHandle h,byte[] b,uint n,out uint d,IntPtr o);
  [DllImport("kernel32.dll",SetLastError=true)] static extern bool ReadFile(SafeFileHandle h,byte[] b,uint n,out uint d,IntPtr o);
  SafeFileHandle handle;
  static void Check(bool ok) { if(!ok)throw new Win32Exception(Marshal.GetLastWin32Error()); }
  public WinCom(string port,int baud) {
   handle=CreateFile(@"\\.\"+port,0xC0000000,0,IntPtr.Zero,3,0,IntPtr.Zero);
   if(handle.IsInvalid)throw new IOException("Cannot open "+port+". Close HTerm/MCUISP and check the configured port.",new Win32Exception(Marshal.GetLastWin32Error()));
   try {
    Dcb d=new Dcb();d.Length=(uint)Marshal.SizeOf(typeof(Dcb));Check(GetCommState(handle,ref d));
    d.Baud=(uint)baud;d.Flags=3;d.ByteSize=8;d.Parity=2;d.StopBits=0;
    Check(SetCommState(handle,ref d));
    Timeouts t=new Timeouts{Interval=uint.MaxValue,ReadConst=100,WriteConst=2000};
    Check(SetCommTimeouts(handle,ref t));
   } catch { handle.Dispose();throw; }
  }
  public void Lines(bool rts,bool dtr) {
   Check(EscapeCommFunction(handle,rts?3u:4u));
   Check(EscapeCommFunction(handle,dtr?5u:6u));
  }
  public void Purge(){Check(PurgeComm(handle,12));}
  public void Write(byte[] data){uint n;Check(WriteFile(handle,data,(uint)data.Length,out n,IntPtr.Zero));if(n!=data.Length)throw new IOException("Short UART write");}
  public byte[] Read(int count,int timeoutMs){
   byte[] result=new byte[count];int offset=0;DateTime deadline=DateTime.UtcNow.AddMilliseconds(timeoutMs);
   while(offset<count){
    if(DateTime.UtcNow>=deadline)throw new TimeoutException("STM32 bootloader did not reply");
    byte[] part=new byte[count-offset];uint n;
    Check(ReadFile(handle,part,(uint)part.Length,out n,IntPtr.Zero));
    Array.Copy(part,0,result,offset,(int)n);offset+=(int)n;
   }
   return result;
  }
  public void Dispose(){if(handle!=null)handle.Dispose();}
 }

 public sealed class Image {
  public const uint Base=0x08000000;
  public const int Capacity=65536;
  public byte[] Bytes; public int Length;
  public static Image Load(string filename) {
   SortedDictionary<uint,byte> map=new SortedDictionary<uint,byte>();uint upper=0;bool eof=false;
   foreach(string raw in File.ReadAllLines(filename)){
    string line=raw.Trim();if(line.Length==0)continue;
    if(eof)throw new FormatException("Data after Intel HEX EOF");
    if(!line.StartsWith(":"))throw new FormatException("Not Intel HEX");
    string hex=line.Substring(1);if(hex.Length%2!=0)throw new FormatException("Odd HEX length");
    byte[] r=new byte[hex.Length/2];int sum=0;
    for(int i=0;i<r.Length;i++){r[i]=byte.Parse(hex.Substring(i*2,2),NumberStyles.HexNumber);sum+=r[i];}
    if(r.Length<5||r.Length!=r[0]+5||(sum&255)!=0)throw new FormatException("Bad Intel HEX length/checksum");
    int n=r[0];uint off=(uint)((r[1]<<8)|r[2]);
    switch(r[3]){
     case 0:
      for(int i=0;i<n;i++){
       ulong a=(ulong)upper+off+(uint)i;
       if(a<Base||a>=Base+Capacity)throw new FormatException("HEX address outside STM32F103C8 application Flash");
       uint address=(uint)a;
       if(map.ContainsKey(address)&&map[address]!=r[4+i])throw new FormatException("Conflicting HEX records");
       map[address]=r[4+i];
      }break;
     case 1:if(n!=0)throw new FormatException("Bad EOF");eof=true;break;
     case 2:if(n!=2||off!=0)throw new FormatException("Bad segment record");upper=(uint)((r[4]<<8)|r[5])<<4;break;
     case 4:if(n!=2||off!=0)throw new FormatException("Bad address record");upper=(uint)((r[4]<<8)|r[5])<<16;break;
     case 3:case 5:if(n!=4)throw new FormatException("Bad start record");break;
     default:throw new FormatException("Unsupported HEX record");
    }
   }
   if(!eof||map.Count<8)throw new FormatException("Incomplete HEX file");
   for(uint i=0;i<8;i++)if(!map.ContainsKey(Base+i))throw new FormatException("Missing Cortex-M vectors");
   uint last=Base;foreach(uint a in map.Keys)last=a;
   int length=(int)(last-Base+1);byte[] bytes=new byte[(length+255)&~255];
   for(int i=0;i<bytes.Length;i++)bytes[i]=255;
   foreach(KeyValuePair<uint,byte> item in map)bytes[(int)(item.Key-Base)]=item.Value;
   uint sp=BitConverter.ToUInt32(bytes,0),reset=BitConverter.ToUInt32(bytes,4);
   if(sp<0x20000000||sp>0x20005000||(sp&7)!=0)throw new FormatException("Invalid C8 initial stack");
   if((reset&1)==0||reset<Base||(ulong)reset>=Base+(ulong)length)throw new FormatException("Invalid reset vector");
   return new Image{Bytes=bytes,Length=length};
  }
 }

 public sealed class Bootloader {
  IWire wire;public Bootloader(IWire w){wire=w;}
  void Ack(int timeout){byte b=wire.Read(1,timeout)[0];if(b!=0x79)throw new IOException("Expected STM32 ACK, got 0x"+b.ToString("X2"));}
  void Command(byte cmd){wire.Write(new byte[]{cmd,(byte)(cmd^255)});Ack(2000);}
  void Address(uint address){byte[] b={(byte)(address>>24),(byte)(address>>16),(byte)(address>>8),(byte)address,0};b[4]=(byte)(b[0]^b[1]^b[2]^b[3]);wire.Write(b);Ack(2000);}
  public bool Sync(){try{wire.Write(new byte[]{0x7f});Ack(500);return true;}catch(TimeoutException){return false;}catch(IOException){return false;}}
  public ushort Id(){Command(2);int n=wire.Read(1,2000)[0]+1;byte[] b=wire.Read(n,2000);Ack(2000);if(n!=2)throw new IOException("Unexpected PID length");return (ushort)((b[0]<<8)|b[1]);}
  public byte[] Commands(){Command(0);int n=wire.Read(1,2000)[0]+1;byte[] b=wire.Read(n,2000);Ack(2000);Console.WriteLine("Bootloader "+(b[0]>>4)+"."+(b[0]&15));return b;}
  public byte[] Read(uint address,int count){
   if(count<1||count>256)throw new ArgumentOutOfRangeException("count");
   Command(0x11);Address(address);byte n=(byte)(count-1);wire.Write(new byte[]{n,(byte)(n^255)});Ack(2000);return wire.Read(count,3000);
  }
  public void ErasePages(int pages){
   if(pages<1||pages>64)throw new ArgumentOutOfRangeException("pages");
   Command(0x43);byte[] packet=new byte[pages+2];packet[0]=(byte)(pages-1);byte xor=packet[0];
   for(int i=0;i<pages;i++){packet[i+1]=(byte)i;xor^=(byte)i;}
   packet[packet.Length-1]=xor;wire.Write(packet);Ack(30000);
  }
  public void Write(uint address,byte[] data){
   if(data.Length<1||data.Length>256||data.Length%4!=0||address%4!=0)throw new ArgumentException("Invalid Flash block");
   Command(0x31);Address(address);byte[] p=new byte[data.Length+2];p[0]=(byte)(data.Length-1);byte xor=p[0];
   Array.Copy(data,0,p,1,data.Length);foreach(byte b in data)xor^=b;p[p.Length-1]=xor;wire.Write(p);Ack(3000);
  }
  public void Go(){Command(0x21);Address(Image.Base);}
 }

 public static class Programmer {
  public static void Run(string filename,string port,int baud,bool bootRts,bool resetDtr,int waitSeconds,bool probeOnly,string backupDirectory) {
   Image image=probeOnly?null:Image.Load(filename); // Validate before opening or resetting anything.
   using(IWire wire=new WinCom(port,baud)){
    Bootloader boot=new Bootloader(wire);bool connected=false;
    Console.WriteLine("Port: "+port+" @ "+baud+" 8E1");
    wire.Lines(bootRts,resetDtr);Thread.Sleep(100);
    wire.Lines(bootRts,!resetDtr);Thread.Sleep(100);wire.Purge();
    for(int i=0;i<3&&!connected;i++)connected=boot.Sync();
    if(!connected&&waitSeconds>0){
     Console.WriteLine("AUTO RESET: no ACK. Press and release the physical RST button now ("+waitSeconds+" seconds).");
     DateTime deadline=DateTime.UtcNow.AddSeconds(waitSeconds);
     while(DateTime.UtcNow<deadline&&!connected)connected=boot.Sync();
    }
    if(!connected)throw new IOException("No bootloader ACK. Check RTS/DTR profile and press RST on the next attempt.");
    ushort id=boot.Id();Console.WriteLine("PID: 0x"+id.ToString("X4"));
    if(id!=0x410)throw new IOException("Unexpected MCU; expected STM32F1 medium-density PID 0x0410. No Flash changes made.");
    byte[] cmds=boot.Commands();
    foreach(byte required in new byte[]{0x11,0x21,0x31,0x43})
     if(Array.IndexOf(cmds,required)<0)throw new IOException("Required command not supported: 0x"+required.ToString("X2"));
    byte[] size=boot.Read(0x1ffff7e0,2);int flashKb=size[0]|(size[1]<<8);
    Console.WriteLine("Flash size: "+flashKb+" KiB");
    if(flashKb!=64&&flashKb!=128)throw new IOException("Unexpected Flash size; no Flash changes made.");
    if(probeOnly){Console.WriteLine("PROBE OK: no erase or programming performed.");return;}
    Directory.CreateDirectory(backupDirectory);
    string backup=Path.Combine(backupDirectory,port+"-"+DateTime.Now.ToString("yyyyMMdd-HHmmss-fff")+".bin");
    using(FileStream file=File.Create(backup))for(uint a=0;a<Image.Capacity;a+=256){byte[] block=boot.Read(Image.Base+a,256);file.Write(block,0,block.Length);}
    Console.WriteLine("Backup: "+backup);
    int pages=(image.Bytes.Length+1023)/1024;
    Console.WriteLine("Erase application pages: "+pages);boot.ErasePages(pages);
    for(int offset=0;offset<image.Bytes.Length;offset+=256){
     byte[] block=new byte[256];Array.Copy(image.Bytes,offset,block,0,256);boot.Write(Image.Base+(uint)offset,block);
    }
    Console.WriteLine("Write OK: "+image.Length+" bytes");
    for(int offset=0;offset<image.Bytes.Length;offset+=256){
     byte[] block=boot.Read(Image.Base+(uint)offset,256);
     for(int i=0;i<256;i++)if(block[i]!=image.Bytes[offset+i])throw new IOException("Verify mismatch at 0x"+(Image.Base+(uint)offset+(uint)i).ToString("X8"));
    }
    Console.WriteLine("Verify OK");boot.Go();Console.WriteLine("Run OK at 0x08000000");
   }
  }
 }
}
