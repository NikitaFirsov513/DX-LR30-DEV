$ErrorActionPreference = 'Stop'
$tests = @'
namespace Pj26Link.Tests {
using System;
using System.Text;
using System.Collections.Generic;
using Pj26Link;
public static class LinkTests {
    static void Require(bool condition, string message) { if (!condition) throw new Exception(message); }
    static byte[] Join(params byte[][] arrays) {
        List<byte> bytes=new List<byte>(); foreach(byte[] data in arrays) bytes.AddRange(data); return bytes.ToArray();
    }
    static byte[] id={1,2,3,4,5,6,7,8};
    static void Parser() {
        byte[] check=Encoding.ASCII.GetBytes("123456789");
        Require(Frame.Crc32(check,check.Length)==0xcbf43926u,"CRC32 reference vector");
        foreach(int size in new int[]{24,64,255}) {
            byte[] frame=Frame.Create(size,id,0,1);
            StreamParser parser=new StreamParser();
            List<byte[]> frames=new List<byte[]>();
            foreach(byte b in frame) frames.AddRange(parser.Feed(new byte[]{b}));
            Require(frames.Count==1 && Frame.Same(frames[0],frame),"Fragmented binary frame");
        }
        byte[] good=Frame.Create(64,id,1,2), bad=(byte[])good.Clone(); bad[20]^=1;
        byte[] bogus={0x50,0x4a,0x4c,0x31,255,0,0};
        StreamParser stream=new StreamParser();
        List<byte[]> parsed=stream.Feed(Join(Encoding.ASCII.GetBytes("PJ26 CMSIS\r\n[diag] noise\r\n"),bad,bogus,good,good));
        Require(parsed.Count==2 && stream.BadFrames==1,"Resync after corruption, noise and incomplete false header");
        Require(Frame.Same(parsed[0],good),"Payload unchanged");
        stream.Feed(new byte[16384]);
        Require(stream.Feed(good).Count==1,"Bounded parser recovers after long noise");
        bool rejected=false; try{Frame.Create(23,id,0,1);}catch(ArgumentException){rejected=true;}
        Require(rejected,"Minimum packet size");
        rejected=false; try{new Options{Size=256}.Validate();}catch(ArgumentException){rejected=true;}
        Require(rejected,"Maximum packet size");
    }
    static Session Make(MemoryEndpoint a,MemoryEndpoint b,int count) {
        return new Session(a,b,new Options{Count=count,StartupMs=0,TailMs=100,GapMs=100,TimeoutMs=1000},"A","B");
    }
    static void Finish(Session test,long start) {
        for(long now=start;now<20000 && !test.Report.Completed;now+=10) test.Step(now);
        Require(test.Report.Completed,"Runner did not finish");
    }
    static void NormalAndEcho() {
        MemoryEndpoint a=new MemoryEndpoint(), b=new MemoryEndpoint();a.Peer=b;b.Peer=a;
        Session test=Make(a,b,3); Finish(test,0);
        Require(test.Report.Passed && test.Report.Packets.Count==6,"Both directions must be checked");
        foreach(DirectionResult result in test.Report.Directions)
            Require(result.Sent==3 && result.Received==3 && result.Lost==0 && result.MinMs==10,"Delivery and PC latency");
        a=new MemoryEndpoint(); b=new MemoryEndpoint();
        a.OnWrite=delegate(byte[] frame){a.Inject(frame);};
        b.OnWrite=delegate(byte[] frame){b.Inject(frame);};
        test=Make(a,b,1); Finish(test,0);
        Require(!test.Report.Passed && test.Report.Directions[0].Lost==1 && test.Report.Directions[1].Lost==1,"UART echo must never pass a radio test");
        Require(test.Report.Directions[0].Echoes==1 && test.Report.Directions[1].Echoes==1,"Local echoes diagnosed");
    }
    static void ErrorsAndLate() {
        MemoryEndpoint a=new MemoryEndpoint(), b=new MemoryEndpoint();
        a.OnWrite=delegate(byte[] frame){
            b.Inject(frame); b.Inject(frame);
            byte[] foreign=Frame.Create(64,new byte[8],0,1);b.Inject(foreign);
        };
        b.Peer=a;
        Session test=Make(a,b,1); Finish(test,0);
        Require(test.Report.Directions[0].Received==1 && test.Report.Directions[0].Duplicates==1,"Duplicates do not inflate delivery");
        Require(test.Report.ForeignFrames==1 && !test.Report.Passed,"Foreign session cannot pass");

        a=new MemoryEndpoint();b=new MemoryEndpoint();
        a.OnWrite=delegate(byte[] frame){
            byte[] bad=(byte[])frame.Clone();bad[20]^=1;b.Inject(bad);
            byte[] mismatch=(byte[])frame.Clone();mismatch[20]^=1;Frame.Write32(mismatch,60,Frame.Crc32(mismatch,60));b.Inject(mismatch);
        };
        b.Peer=a;
        test=Make(a,b,1); Finish(test,0);
        Require(test.Report.BadTestFrames[1]==1 && test.Report.Directions[0].Mismatches==1,"CRC32 and full-byte comparisons");
        Require(test.Report.Directions[0].Lost==1 && !test.Report.Passed,"Corruption is not delivery");

        a=new MemoryEndpoint();b=new MemoryEndpoint();byte[] delayed=null;
        a.OnWrite=delegate(byte[] frame){delayed=frame;}; b.Peer=a;
        test=Make(a,b,1);test.Step(0);test.Step(1000);b.Inject(delayed);test.Step(1010);Finish(test,1020);
        Require(test.Report.Directions[0].Received==1 && test.Report.Directions[0].Late==1,"Late packet still counted uniquely");
        Require(test.Report.Directions[0].Lost==0 && test.Report.Packets[0].Status=="late" && !test.Report.Passed,"Late delivery does not silently pass the deadline");
    }
    static void StartupAndFailure() {
        MemoryEndpoint a=new MemoryEndpoint(), b=new MemoryEndpoint();a.Peer=b;b.Peer=a;
        Session test=new Session(a,b,new Options{Count=1,StartupMs=1500,TailMs=0},"A","B");
        a.Inject(Encoding.ASCII.GetBytes("Radio ready\r\n"));test.Step(0);test.Step(1490);
        Require(test.Report.Packets.Count==0,"No transmission during startup guard");
        test.Step(1500);Require(test.Report.Directions[0].Sent==1,"Start after guard");
        Require(!test.Report.Completed && !test.Report.Passed,"Partial runs cannot pass");
        a=new MemoryEndpoint();b=new MemoryEndpoint();a.OnWrite=delegate(byte[] frame){throw new InvalidOperationException("unplugged");};
        test=Make(a,b,1);bool failed=false;try{test.Step(0);}catch(InvalidOperationException){failed=true;}
        Require(failed && test.Report.Directions[0].Sent==0 && !test.Report.Passed,"Transport error cannot invent a successful write");
    }
    static void Reports() {
        string directory=System.IO.Path.Combine(System.IO.Path.GetTempPath(),"pj26-link-"+Guid.NewGuid().ToString("N"));
        System.IO.Directory.CreateDirectory(directory);
        string file=System.IO.Path.Combine(directory,"report.json");
        try {
            ReportFile.Save(file,"{\"Completed\":false}");
            ReportFile.Save(file,"{\"Completed\":true}");
            Require(System.IO.File.ReadAllText(file)=="{\"Completed\":true}","Atomic checkpoint replacement");
            Require(!System.IO.File.Exists(file+".tmp"),"Checkpoint temp file not consumed");
        } finally {
            if(System.IO.File.Exists(file))System.IO.File.Delete(file);
            if(System.IO.File.Exists(file+".tmp"))System.IO.File.Delete(file+".tmp");
            System.IO.Directory.Delete(directory);
        }
    }
    public static void Run() {
        Parser();NormalAndEcho();ErrorsAndLate();StartupAndFailure();Reports();
        Console.WriteLine("PASS: link test CRC32/framing/resync, both directions, local echo, loss/corruption/duplicate/late/foreign packets, latency, startup and transport failures (no COM access)");
    }
}
}
'@
$source = Get-Content -LiteralPath (Join-Path $PSScriptRoot 'RadioLinkTest.cs') -Raw
Add-Type -TypeDefinition ($source + "`n" + $tests)
[Pj26Link.Tests.LinkTests]::Run()
