using System;
using System.Collections.Generic;
using System.IO;
using System.Net;
using System.Net.Sockets;
using System.Text;
using System.Threading.Tasks;

public static class TaskHttpDownloader
{
    private static Task ConnectTask(Socket socket, EndPoint ep)
    {
        var tcs = new TaskCompletionSource<bool>();
        socket.BeginConnect(ep, ar =>
        {
            try
            {
                socket.EndConnect(ar);
                tcs.SetResult(true);
            }
            catch (Exception ex)
            {
                tcs.SetException(ex);
            }
        }, null);
        return tcs.Task;
    }

    private static Task<int> SendTask(Socket socket, byte[] buffer, int offset, int size)
    {
        var tcs = new TaskCompletionSource<int>();
        socket.BeginSend(buffer, offset, size, SocketFlags.None, ar =>
        {
            try
            {
                int sent = socket.EndSend(ar);
                tcs.SetResult(sent);
            }
            catch (Exception ex)
            {
                tcs.SetException(ex);
            }
        }, null);
        return tcs.Task;
    }

    private static Task<int> ReceiveTask(Socket socket, byte[] buffer, int offset, int size)
    {
        var tcs = new TaskCompletionSource<int>();
        socket.BeginReceive(buffer, offset, size, SocketFlags.None, ar =>
        {
            try
            {
                int received = socket.EndReceive(ar);
                tcs.SetResult(received);
            }
            catch (Exception ex)
            {
                tcs.SetException(ex);
            }
        }, null);
        return tcs.Task;
    }

    private class State
    {
        public Uri Uri;
        public string OutputFile;
        public Socket Socket;
        public byte[] Buffer = new byte[8192];

        public List<byte> HeaderBuffer = new List<byte>();
        public bool HeadersParsed = false;
        public int ContentLength = -1;
        public int BodyBytesRead = 0;
        public FileStream FileStream;
    }

    public static Task DownloadWithTasks(string url, string outputPath)
    {
        Console.WriteLine($"[tasks] Start: {url} -> {outputPath}");

        Uri uri = new Uri(url);
        var ep = Utils.CreateEndPoint(uri);
        var socket = new Socket(AddressFamily.InterNetwork, SocketType.Stream, ProtocolType.Tcp);

        var state = new State
        {
            Uri = uri,
            OutputFile = outputPath,
            Socket = socket
        };

        string request = Utils.BuildRequest(uri);
        byte[] requestBytes = Encoding.ASCII.GetBytes(request);

        return ConnectTask(socket, ep)
            .ContinueWith(t =>
            {
                if (t.IsFaulted) throw t.Exception.InnerException;
                return SendTask(socket, requestBytes, 0, requestBytes.Length);
            }).Unwrap()
            .ContinueWith(t =>
            {
                if (t.IsFaulted) throw t.Exception.InnerException;
                return ReceiveLoop(state);
            }).Unwrap()
            .ContinueWith(t =>
            {
                try { state.FileStream?.Close(); } catch { }
                try
                {
                    state.Socket?.Shutdown(SocketShutdown.Both);
                }
                catch { }
                try { state.Socket?.Close(); } catch { }

                if (t.IsFaulted)
                {
                    Console.WriteLine($"[tasks] Error downloading {url}: {t.Exception.InnerException.Message}");
                    throw t.Exception.InnerException;
                }
                else
                {
                    Console.WriteLine($"[tasks] Finished: {url} -> {outputPath}");
                }
            });
    }

    private static Task ReceiveLoop(State state)
    {
        return ReceiveTask(state.Socket, state.Buffer, 0, state.Buffer.Length)
            .ContinueWith(t =>
            {
                if (t.IsFaulted) throw t.Exception.InnerException;

                int n = t.Result;
                if (n <= 0)
                {
                    return Task.CompletedTask;
                }

                if (!state.HeadersParsed)
                {
                    for (int i = 0; i < n; i++)
                        state.HeaderBuffer.Add(state.Buffer[i]);

                    byte[] all = state.HeaderBuffer.ToArray();
                    int headerEnd = Utils.FindHeaderEnd(all, all.Length);

                    if (headerEnd >= 0)
                    {
                        string headerString = Encoding.ASCII.GetString(all, 0, headerEnd);
                        state.ContentLength = Utils.ParseContentLength(headerString);

                        Console.WriteLine("[tasks] Headers:");
                        Console.WriteLine(headerString);
                        Console.WriteLine($"[tasks] Content-Length: {state.ContentLength}");

                        state.FileStream = new FileStream(state.OutputFile, FileMode.Create, FileAccess.Write);

                        int bodyStart = headerEnd + 4;
                        int bodyLenInAccumulator = all.Length - bodyStart;
                        if (bodyLenInAccumulator > 0)
                        {
                            state.FileStream.Write(all, bodyStart, bodyLenInAccumulator);
                            state.BodyBytesRead += bodyLenInAccumulator;
                        }

                        state.HeadersParsed = true;
                        state.HeaderBuffer.Clear();
                    }
                }
                else
                {
                    state.FileStream.Write(state.Buffer, 0, n);
                    state.BodyBytesRead += n;
                }

                if (state.HeadersParsed && state.ContentLength > 0 && state.BodyBytesRead >= state.ContentLength)
                {
                    return Task.CompletedTask;
                }

                return ReceiveLoop(state);
            }).Unwrap();
    }
}