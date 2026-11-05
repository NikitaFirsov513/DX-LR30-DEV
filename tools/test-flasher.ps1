$ErrorActionPreference='Stop'
$tests = @'
namespace Pj26Flash.Tests {
using System;
using System.IO;
using System.Text;
using System.Collections.Generic;
using Pj26Flash;
public sealed class FakeStm32 : IWire {
 public byte[] Memory=new byte[65536];
 Queue<byte> output=new Queue<byte>();int phase=0;byte command;uint address;
 public FakeStm32(){for(int i=0;i<Memory.Length;i++)Memory[i]=0x55;}
 void Emit(params byte[] b){foreach(byte x in b)output.Enqueue(x);}
 static void Require(bool value){if(!value)throw new Exception("Invalid host bootloader transaction");}
 static bool XorOk(byte[] b){byte x=0;foreach(byte a in b)x^=a;return x==0;}
 public void Write(byte[] b){
  if(phase==0){
   if(b.Length==1&&b[0]==0x7f){Emit(0x79);return;}
   Require(b.Length==2&&(b[0]^b[1])==255);command=b[0];Emit(0x79);
   if(command==2){Emit(1,4,0x10,0x79);return;}
   if(command==0){Emit(7,0x22,0,1,2,0x11,0x21,0x31,0x43,0x79);return;}
   if(command==0x43){phase=3;return;}
   Require(command==0x11||command==0x31||command==0x21);phase=1;return;
  }
  if(phase==1){
   Require(b.Length==5&&XorOk(b));
   address=((uint)b[0]<<24)|((uint)b[1]<<16)|((uint)b[2]<<8)|b[3];
   Emit(0x79);phase=command==0x21?0:2;return;
  }
  if(phase==3){
   Require(b.Length==b[0]+3&&XorOk(b));
   for(int i=1;i<b.Length-1;i++){
    Require(b[i]<64);for(int j=0;j<1024;j++)Memory[b[i]*1024+j]=255;
   }Emit(0x79);phase=0;return;
  }
  if(command==0x11){
   Require(b.Length==2&&(b[0]^b[1])==255);int count=b[0]+1;Emit(0x79);
   if(address==0x1ffff7e0){Require(count==2);Emit(64,0);}
   else{Require(address>=Image.Base&&(ulong)address+(uint)count<=(ulong)Image.Base+(uint)Memory.Length);
    for(int i=0;i<count;i++)Emit(Memory[(int)(address-Image.Base)+i]);}
  }else{
   Require(command==0x31&&b.Length==b[0]+3&&XorOk(b));int count=b[0]+1;
   Require(address%4==0&&count%4==0&&address>=Image.Base&&(ulong)address+(uint)count<=(ulong)Image.Base+(uint)Memory.Length);
   for(int i=0;i<count;i++){
    int offset=(int)(address-Image.Base)+i;Require((Memory[offset]&b[i+1])==b[i+1]);Memory[offset]=b[i+1];
   }Emit(0x79);
  }phase=0;
 }
 public byte[] Read(int n,int timeout){if(output.Count<n)throw new TimeoutException();byte[] b=new byte[n];for(int i=0;i<n;i++)b[i]=output.Dequeue();return b;}
 public void Lines(bool r,bool d){}
 public void Purge(){output.Clear();}
 public void Dispose(){}
}
public static class FlasherTests {
 static void Require(bool v,string m){if(!v)throw new Exception(m);}
 static string Record(ushort off,byte type,byte[] data){
  List<byte> b=new List<byte>();b.Add((byte)data.Length);b.Add((byte)(off>>8));b.Add((byte)off);b.Add(type);b.AddRange(data);
  int sum=0;foreach(byte x in b)sum+=x;b.Add((byte)(-sum));StringBuilder s=new StringBuilder(":");foreach(byte x in b)s.Append(x.ToString("X2"));return s.ToString();
 }
 public static void Run(string directory){
  Directory.CreateDirectory(directory);string file=Path.Combine(directory,"test.hex");
  byte[] data={0,0x50,0,0x20,9,0,0,8,0,0,0,0,0xAA,0x55,0x12,0x34};
  string good=Record(0,4,new byte[]{8,0})+"\n"+Record(0,0,data)+"\n"+Record(0,1,new byte[0])+"\n";
  File.WriteAllText(file,good);Image image=Image.Load(file);
  Require(image.Length==16&&image.Bytes.Length==256&&image.Bytes[16]==255,"HEX padding");
  using(var wire=new FakeStm32()){
   var boot=new Bootloader(wire);Require(boot.Sync(),"sync");Require(boot.Id()==0x410,"PID");
   Require(boot.Commands()[0]==0x22,"version");Require(boot.Read(0x1ffff7e0,2)[0]==64,"size");
   boot.ErasePages(1);for(int i=0;i<1024;i++)Require(wire.Memory[i]==255,"page erase");
   Require(wire.Memory[1024]==0x55,"erase touched an unrelated page");
   byte[] block=new byte[256];for(int i=0;i<256;i++)block[i]=(byte)(i^0xA5);
   boot.Write(Image.Base,block);byte[] back=boot.Read(Image.Base,256);
   for(int i=0;i<256;i++)Require(back[i]==block[i],"read-back mismatch");
   bool rejected=false;try{boot.Write(Image.Base+1,block);}catch(ArgumentException){rejected=true;}Require(rejected,"unaligned write accepted");
   boot.Go();
  }
  string[] bad={good.Replace(":020000040800F2",":020000040800F3"),good.Replace(":020000040800F2",Record(0,4,new byte[]{0x1F,0xFF})),good.Replace(":00000001FF","")};
  foreach(string text in bad){File.WriteAllText(file,text);bool rejected=false;try{Image.Load(file);}catch(FormatException){rejected=true;}Require(rejected,"invalid HEX accepted");}
  Console.WriteLine("PASS: HEX parsing/checksums/range/vectors; sync/ID/version; page erase; 256-byte write/read; bounds; GO");
 }
}
}
'@
$source = Get-Content -LiteralPath (Join-Path $PSScriptRoot 'Stm32Serial.cs') -Raw
Add-Type -TypeDefinition ($source + "`n" + $tests)
[Pj26Flash.Tests.FlasherTests]::Run((Join-Path (Split-Path -Parent $PSScriptRoot) 'build/flasher-tests'))
