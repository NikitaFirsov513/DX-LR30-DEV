// PC-side test payloads only. This does not change the firmware's air protocol.
using System;
using System.Collections.Generic;
using System.IO;
using System.IO.Ports;
using System.Text;

namespace Pj26Link {
    public static class ReportFile {
        public static void Save(string path, string json) {
            string temporary = path + ".tmp";
            File.WriteAllText(temporary, json, new UTF8Encoding(false));
            if (File.Exists(path)) File.Replace(temporary, path, null);
            else File.Move(temporary, path);
        }
    }

    public interface IEndpoint {
        byte[] ReadAvailable();
        void Write(byte[] data);
    }

    public sealed class SerialEndpoint : IEndpoint, IDisposable {
        readonly SerialPort port;
        public SerialEndpoint(string name, int baud, bool dtr, bool rts) {
            port = new SerialPort(name, baud, Parity.None, 8, StopBits.One);
            port.Handshake = Handshake.None;
            port.DtrEnable = dtr; port.RtsEnable = rts;
            port.ReadTimeout = 100; port.WriteTimeout = 2000; port.ReadBufferSize = 8192;
            try { port.Open(); } catch { port.Dispose(); throw; }
        }
        public byte[] ReadAvailable() {
            int count = Math.Min(port.BytesToRead, 4096);
            if (count == 0) return new byte[0];
            byte[] data = new byte[count];
            int read = port.Read(data, 0, count);
            if (read != count) Array.Resize(ref data, read);
            return data;
        }
        public void Write(byte[] data) { port.Write(data, 0, data.Length); }
        public void Dispose() { port.Dispose(); }
    }

    // Used only by the explicitly labelled preview and host tests; never a hardware fallback.
    public sealed class MemoryEndpoint : IEndpoint {
        readonly List<byte> pending = new List<byte>();
        public MemoryEndpoint Peer;
        public Action<byte[]> OnWrite;
        public void Inject(byte[] bytes) { pending.AddRange(bytes); }
        public byte[] ReadAvailable() { byte[] result = pending.ToArray(); pending.Clear(); return result; }
        public void Write(byte[] data) {
            if (OnWrite != null) OnWrite(data);
            else if (Peer != null) Peer.Inject(data);
        }
    }

    public static class Frame {
        public const int MinimumSize = 24;
        public static uint Crc32(byte[] data, int count) {
            uint crc = 0xffffffff;
            for (int i = 0; i < count; i++) {
                crc ^= data[i];
                for (int bit = 0; bit < 8; bit++) crc = (crc >> 1) ^ ((crc & 1) != 0 ? 0xedb88320u : 0u);
            }
            return ~crc;
        }
        public static uint Read32(byte[] data, int offset) {
            return (uint)data[offset] | ((uint)data[offset+1] << 8) |
                   ((uint)data[offset+2] << 16) | ((uint)data[offset+3] << 24);
        }
        public static void Write32(byte[] data, int offset, uint value) {
            for (int i = 0; i < 4; i++) data[offset+i] = (byte)(value >> (i * 8));
        }
        public static byte[] Create(int size, byte[] session, int direction, int sequence) {
            if (size < MinimumSize || size > 255 || session.Length != 8 || direction < 0 || direction > 1 || sequence < 1)
                throw new ArgumentException("Invalid link-test frame parameters.");
            byte[] data = new byte[size];
            data[0]=0x50; data[1]=0x4a; data[2]=0x4c; data[3]=0x31; // PJL1
            data[4]=(byte)size; data[5]=(byte)direction;
            Array.Copy(session, 0, data, 6, 8);
            Write32(data, 14, (uint)sequence);
            for (int i = 18; i < size-4; i++) data[i]=(byte)(i * 73 + sequence * 17 + direction * 41);
            Write32(data, size-4, Crc32(data, size-4));
            return data;
        }
        public static bool Same(byte[] left, byte[] right) {
            if (left.Length != right.Length) return false;
            for (int i = 0; i < left.Length; i++) if (left[i] != right[i]) return false;
            return true;
        }
    }

    public sealed class StreamParser {
        readonly List<byte> buffer = new List<byte>();
        public int BadFrames;
        public List<byte[]> Feed(byte[] data) {
            buffer.AddRange(data);
            List<byte[]> frames = new List<byte[]>();
            int offset = 0;
            while (offset + 5 <= buffer.Count) {
                if (buffer[offset] != 0x50 || buffer[offset+1] != 0x4a || buffer[offset+2] != 0x4c || buffer[offset+3] != 0x31) {
                    offset++; continue;
                }
                int size = buffer[offset+4];
                if (size < Frame.MinimumSize) { offset++; continue; }
                // An incomplete false header must not hide a later complete valid frame.
                if (offset + size > buffer.Count) { offset++; continue; }
                byte[] frame = buffer.GetRange(offset, size).ToArray();
                if (Frame.Read32(frame, size-4) != Frame.Crc32(frame, size-4)) {
                    BadFrames++; buffer.RemoveRange(0, offset+1); offset=0; continue;
                }
                frames.Add(frame); buffer.RemoveRange(0, offset+size); offset=0;
            }
            if (buffer.Count > 254) buffer.RemoveRange(0, buffer.Count-254);
            return frames;
        }
    }

    public sealed class Options {
        public int Count=20, Size=64, Baud=9600, TimeoutMs=4000, GapMs=200, StartupMs=1500, TailMs=1000;
        public void Validate() {
            if (Count < 1 || Count > 10000 || Size < Frame.MinimumSize || Size > 255 || Baud != 9600 ||
                TimeoutMs < 1000 || TimeoutMs > 60000 || GapMs < 100 || GapMs > 60000 ||
                StartupMs < 0 || StartupMs > 10000 || TailMs < 0 || TailMs > 60000)
                throw new ArgumentException("Invalid test options: count 1..10000, size 24..255, baud 9600, timeout 1000..60000 ms, gap 100..60000 ms.");
        }
    }

    public sealed class PacketResult {
        public string Direction, Status="waiting";
        public int Sequence;
        public long? LatencyMs;
        internal byte[] Expected;
        internal long Started;
    }
    public sealed class DirectionResult {
        public string Direction;
        public int Sent, Received, Late, Duplicates, Echoes, Mismatches;
        public long? MinMs, MaxMs;
        internal long TotalMs;
        public int Lost { get { return Sent - Received; } }
        public double LossPercent { get { return Sent == 0 ? 0 : 100.0 * Lost / Sent; } }
        public double? AverageMs { get { return Received == 0 ? (double?)null : (double)TotalMs / Received; } }
    }
    public sealed class Result {
        public string Mode="hardware", PortA, PortB, SessionId, StartedUtc=DateTime.UtcNow.ToString("o"), FinishedUtc, Error;
        public bool Completed;
        public Options Options;
        public int ForeignFrames;
        public int[] BadTestFrames = new int[2]; // PC CRC32 errors; not the radio's CRC_ERROR IRQ count.
        public DirectionResult[] Directions = {
            new DirectionResult { Direction="A -> B" }, new DirectionResult { Direction="B -> A" }
        };
        public List<PacketResult> Packets = new List<PacketResult>();
        public bool Passed {
            get {
                if (!Completed || Error != null || BadTestFrames[0] != 0 || BadTestFrames[1] != 0 || ForeignFrames != 0) return false;
                foreach (DirectionResult d in Directions)
                    if (d.Sent != Options.Count || d.Lost != 0 || d.Late != 0 || d.Duplicates != 0 || d.Echoes != 0 || d.Mismatches != 0) return false;
                return true;
            }
        }
    }

    // Incremental runner: PowerShell remains responsive between bounded Step calls (Ctrl+C/finally).
    public sealed class Session {
        readonly IEndpoint[] endpoints;
        readonly StreamParser[] parsers = { new StreamParser(), new StreamParser() };
        readonly byte[] id = new byte[8];
        readonly Queue<string> events = new Queue<string>();
        readonly Dictionary<string, PacketResult> sent = new Dictionary<string, PacketResult>();
        readonly Options options;
        PacketResult current;
        int next;
        long nextSend, finishAt=-1;
        bool started;
        public readonly Result Report;

        public Session(IEndpoint a, IEndpoint b, Options settings, string portA, string portB) {
            settings.Validate(); options=settings; endpoints=new IEndpoint[] { a, b };
            Array.Copy(Guid.NewGuid().ToByteArray(), id, 8);
            Report=new Result { Options=settings, PortA=portA, PortB=portB, SessionId=BitConverter.ToString(id).Replace("-", "") };
        }
        static string Key(int direction, int sequence) { return direction + ":" + sequence; }
        public string[] DrainEvents() { string[] result=events.ToArray(); events.Clear(); return result; }
        void Accept(int receiver, byte[] frame, long now) {
            for (int i=0; i<8; i++) if (frame[6+i] != id[i]) { Report.ForeignFrames++; return; }
            int direction=frame[5]; uint seq=Frame.Read32(frame,14);
            PacketResult packet;
            if (direction > 1 || seq > Int32.MaxValue || !sent.TryGetValue(Key(direction,(int)seq),out packet)) {
                Report.ForeignFrames++; return;
            }
            DirectionResult result=Report.Directions[direction];
            if (receiver == direction) { result.Echoes++; return; }
            if (!Frame.Same(frame,packet.Expected)) { result.Mismatches++; return; }
            if (packet.LatencyMs.HasValue) { result.Duplicates++; return; }
            long elapsed=now-packet.Started;
            packet.LatencyMs=elapsed; packet.Status=elapsed <= options.TimeoutMs ? "ok" : "late";
            result.Received++; if (packet.Status == "late") result.Late++;
            result.TotalMs+=elapsed;
            result.MinMs=result.MinMs.HasValue ? Math.Min(result.MinMs.Value,elapsed) : elapsed;
            result.MaxMs=result.MaxMs.HasValue ? Math.Max(result.MaxMs.Value,elapsed) : elapsed;
        }
        public bool Step(long now) {
            if (Report.Completed) return false;
            for (int receiver=0; receiver<2; receiver++) {
                byte[] data=endpoints[receiver].ReadAvailable();
                if (started) foreach (byte[] frame in parsers[receiver].Feed(data)) Accept(receiver,frame,now);
                Report.BadTestFrames[receiver]=parsers[receiver].BadFrames;
            }
            if (!started) {
                if (now < options.StartupMs) return true;
                started=true;
            }
            if (current != null && (current.LatencyMs.HasValue || now-current.Started >= options.TimeoutMs)) {
                if (!current.LatencyMs.HasValue) current.Status="timeout";
                events.Enqueue(String.Format("{0}  {1,3}/{2}  {3}",current.Direction,current.Sequence,options.Count,
                    current.LatencyMs.HasValue ? current.LatencyMs.Value + " ms" : "TIMEOUT"));
                current=null; nextSend=now+options.GapMs;
                if (next == options.Count*2) finishAt=now+options.TailMs;
            }
            if (next < options.Count*2 && current == null && now >= nextSend) {
                int direction=next/options.Count, sequence=next%options.Count+1;
                byte[] frame=Frame.Create(options.Size,id,direction,sequence);
                endpoints[direction].Write(frame);
                current=new PacketResult { Direction=Report.Directions[direction].Direction, Sequence=sequence, Expected=frame, Started=now };
                Report.Directions[direction].Sent++;
                Report.Packets.Add(current); sent.Add(Key(direction,sequence),current); next++;
            }
            if (finishAt >= 0 && now >= finishAt) {
                Report.Completed=true; Report.FinishedUtc=DateTime.UtcNow.ToString("o"); return false;
            }
            return true;
        }
    }
}
